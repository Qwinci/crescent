#include "allocator.h"
#include "page.h"
#include "pmalloc.h"
#include "sched/mutex.h"
#include "string.h"
#include "utils.h"
#include "vm.h"
#include "utils/math.h"

typedef struct Node {
	struct Node* prev;
	struct Node* next;
} Node;

// 16 32 64 128 256 512 1024 2048
#define FREELIST_COUNT 8

static Node* freelists[FREELIST_COUNT] = {};

static usize index_to_size(usize index) {
	return 16 << index;
}

static usize size_to_index(usize size) {
	if (size <= 16) {
		return 0;
	}
	return sizeof(unsigned long long) * 8 - __builtin_clzll(size) - 5;
}

static void freelist_insert_nonrecursive(usize index, void* ptr) {
	Node* node = (Node*) ptr;
	node->prev = NULL;

	node->next = freelists[index];
	freelists[index] = node;
	if (node->next) {
		node->next->prev = node;
	}

	usize size = index_to_size(index);

	u8* bitmap = (u8*) (ALIGNDOWN((usize) node, PAGE_SIZE));
	usize offset = ((usize) node - (usize) bitmap) / size;
	bitmap[offset / 8] &= ~(1U << (offset % 8));

	for (usize i = 0; i < PAGE_SIZE / size; ++i) {
		if (bitmap[i / 8]) {
			return;
		}
	}

	usize count = PAGE_SIZE / size;
	usize bitmap_size = (count + 7) / 8;

	usize start = ALIGNUP(bitmap_size, size);

	for (usize i = start / size; i < count; ++i) {
		Node* n = offset(bitmap, Node*, i * size);
		if (n->prev) {
			n->prev->next = n->next;
		}
		else {
			freelists[index] = n->next;
		}
		if (n->next) {
			n->next->prev = n->prev;
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

		usize start = ALIGNUP(bitmap_size, size);

		for (usize i = start / size; i < count; ++i) {
			Node* node = (Node*) offset(virt, void*, i * size);
			node->prev = NULL;
			node->next = freelists[index];
			if (node->next) {
				node->next->prev = node;
			}
			freelists[index] = node;
		}
	}

	Node* node = freelists[index];
	freelists[index] = node->next;
	u8* bitmap = (u8*) (ALIGNDOWN((usize) node, PAGE_SIZE));
	usize offset = ((usize) node - (usize) bitmap) / size;
	bitmap[offset / 8] |= 1U << (offset % 8);
	return node;
}

static Mutex MALLOC_LOCK = {};

void* kmalloc(usize size) {
	if (!size) {
		return NULL;
	}
	else if (size == PAGE_SIZE) {
		Page* page = pmalloc(1);
		if (!page) {
			return NULL;
		}
		return to_virt(page->phys);
	}
	else if (size >= 2048) {
		return vm_kernel_alloc_backed(ALIGNUP(size, PAGE_SIZE) / PAGE_SIZE, PF_READ | PF_WRITE | PF_EXEC);
	}
	usize index = size_to_index(size);
	if (size & (size - 1)) {
		index += 1;
	}

	mutex_lock(&MALLOC_LOCK);
	void* ptr = freelist_get_nonrecursive(index, NULL);
	mutex_unlock(&MALLOC_LOCK);
	return ptr;
}

void kfree(void* ptr, usize size) {
	if (!ptr || !size) {
		return;
	}
	else if (size == PAGE_SIZE) {
		pfree(page_from_addr(to_phys(ptr)), 1);
		return;
	}
	if (size >= 2048) {
		return vm_kernel_dealloc_backed(ptr, ALIGNUP(size, PAGE_SIZE) / PAGE_SIZE);
	}

	usize index = size_to_index(size);
	if (size & (size - 1)) {
		index += 1;
	}

	mutex_lock(&MALLOC_LOCK);
	freelist_insert_nonrecursive(index, ptr);
	mutex_unlock(&MALLOC_LOCK);
}

void* kcalloc(usize size) {
	void* mem = kmalloc(size);
	if (!mem) {
		return NULL;
	}
	memset(mem, 0, size);
	return mem;
}

void* krealloc(void* ptr, usize old_size, usize new_size) {
	void* new_ptr = kmalloc(new_size);
	if (!new_ptr) {
		return NULL;
	}
	memcpy(new_ptr, ptr, MIN(old_size, new_size));
	kfree(ptr, old_size);
	return new_ptr;
}
