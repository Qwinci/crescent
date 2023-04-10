#include "allocator.h"
#include "page.h"
#include "pmalloc.h"
#include "string.h"
#include "utils.h"
#include "vm.h"

typedef struct Node {
	struct Node* prev;
	struct Node* next;
} Node;

// 8 16 32 64 128 256 512 1024 2048
#define FREELIST_COUNT 9

static Node* freelists[FREELIST_COUNT] = {};

static usize index_to_size(usize index) {
	return 8 << index;
}

static usize size_to_index(usize size) {
	if (size < 8) {
		return 0;
	}
	return sizeof(unsigned long long) * 8 - __builtin_clzll(size) - 1 - 3;
}

static void freelist_insert_nonrecursive(usize index, void* ptr) {
	Node* node = (Node*) ptr;
	node->prev = NULL;

	node->next = freelists[index];
	freelists[index] = node;

	u8* bitmap = (u8*) (ALIGNDOWN((usize) node, PAGE_SIZE));
	usize offset = (usize) node - (usize) bitmap;
	bitmap[offset / 8] &= ~(1 << (offset % 8));

	usize size = index_to_size(index);

	for (usize i = 0; i < PAGE_SIZE / size; ++i) {
		if (bitmap[i / 8]) {
			return;
		}
	}

	pfree(page_from_addr(to_phys(bitmap)), 1);
}

static void* freelist_get_nonrecursive(usize index, Page** new_page) {
	usize size = index_to_size(index);

	if (!freelists[index]) {
		Page* page = pmalloc(1);
		if (!page) {
			return NULL;
		}
		if (new_page) {
			*new_page = page;
		}
		void* virt = to_virt(page->phys);

		u8* bitmap = (u8*) virt;
		usize count = PAGE_SIZE / size;
		usize bitmap_size = (count + 7) / 8;
		memset(bitmap, 0, bitmap_size);

		usize start = ALIGNUP(bitmap_size, 16);

		for (usize i = start; i < count * size; i += size) {
			Node* node = (Node*) offset(virt, void*, i);
			node->next = freelists[index];
			freelists[index] = node;
		}
	}

	Node* node = freelists[index];
	freelists[index] = node->next;
	u8* bitmap = (u8*) (ALIGNDOWN((usize) node, PAGE_SIZE));
	usize offset = (usize) node - (usize) bitmap;
	bitmap[offset / 8] |= 1 << (offset % 8);
	return node;
}

static Spinlock MALLOC_LOCK = {};

void* kmalloc(usize size) {
	if (!size) {
		return NULL;
	}
	else if (size >= PAGE_SIZE) {
		return vm_kernel_alloc_backed(ALIGNUP(size, PAGE_SIZE) / PAGE_SIZE, PF_READ | PF_WRITE | PF_EXEC);
	}
	usize index = size_to_index(size);
	if (size & (size - 1)) {
		index += 1;
	}

	spinlock_lock(&MALLOC_LOCK);
	void* ptr = freelist_get_nonrecursive(index, NULL);
	spinlock_unlock(&MALLOC_LOCK);
	return ptr;
}

void kfree(void* ptr, usize size) {
	if (!ptr) {
		return;
	}
	else if (size >= PAGE_SIZE) {
		return vm_kernel_dealloc_backed(ptr, ALIGNUP(size, PAGE_SIZE) / PAGE_SIZE);
	}
	usize index = size_to_index(size);
	if (size & (size - 1)) {
		index += 1;
	}

	spinlock_lock(&MALLOC_LOCK);
	freelist_insert_nonrecursive(index, ptr);
	spinlock_unlock(&MALLOC_LOCK);
}