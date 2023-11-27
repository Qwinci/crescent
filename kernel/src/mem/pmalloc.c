#include "pmalloc.h"
#include "arch/misc.h"
#include "page.h"
#include "sched/mutex.h"
#include "stdio.h"
#include "utils.h"
#include "string.h"

// 1 2 4 8 16 32 64 128 256 512 1024 2048 4096 8192
#define FREELIST_COUNT 14

PRegion* pregions = NULL;
PRegion* pregions_end = NULL;

Page* page_freelists[FREELIST_COUNT] = {};

static usize index_to_size(usize index) {
	return 1 << index;
}

static usize size_to_index(usize size) {
	return sizeof(unsigned long long) * 8 - __builtin_clzll(size) - 1;
}

static void freelist_insert_nonrecursive(usize index, Page* page) {
	page->type = PAGE_FREE;
	page->prev = NULL;
	page->next = NULL;
	page->list_index = index;

	usize final_i = index;

	usize new_i = index;
	while (new_i < FREELIST_COUNT) {
		usize count = index_to_size(new_i);
		usize size = count * PAGE_SIZE;

		usize region_free_base = page->region->base + page->region->used_pages * PAGE_SIZE;
		usize buddy = ((page->phys - region_free_base) & ((index_to_size(new_i + 1) * PAGE_SIZE) - 1)) ? page->phys - size : page->phys + size;
		usize buddy_index = (buddy - page->region->base) / PAGE_SIZE;
		if (new_i + 1 == FREELIST_COUNT || buddy_index >= page->region->size) {
			break;
		}

		Page* buddy_page = &page->region->pages[buddy_index];
		if (buddy_page->type == PAGE_FREE && buddy_page->list_index == page->list_index) {
			if (buddy_page->prev) {
				buddy_page->prev->next = buddy_page->next;
			}
			else {
				page_freelists[new_i] = buddy_page->next;
			}
			if (buddy_page->next) {
				buddy_page->next->prev = buddy_page->prev;
			}

			Page* first_page = page < buddy_page ? page : buddy_page;
			first_page->list_index = new_i + 1;

			page = first_page;
			final_i = new_i + 1;
		}
		else {
			break;
		}

		new_i += 1;
	}

	page->prev = NULL;
	page->next = page_freelists[final_i];
	if (page_freelists[final_i]) {
		page_freelists[final_i]->prev = page;
	}
	page_freelists[final_i] = page;
}

static Page* freelist_get_nonrecursive(usize index) {
	if (!page_freelists[index]) {
		if (index + 1 == FREELIST_COUNT) {
			return NULL;
		}

		usize new_i = index + 1;
		while (new_i < FREELIST_COUNT) {
			if (!page_freelists[new_i]) {
				new_i += 1;
				continue;
			}

			Page* page = page_freelists[new_i];
			page_freelists[new_i] = page->next;
			if (page->next) {
				page->next->prev = NULL;
			}
			page->type = PAGE_USED;
			page->list_index = index;

			PRegion* region = page->region;

			for (usize i = new_i; i > index; --i) {
				usize size = index_to_size(i);

				usize page_num = (page->phys - region->base) / PAGE_SIZE;
				Page* second = &region->pages[page_num + size / 2];

				second->prev = NULL;
				second->next = page_freelists[i - 1];
				second->list_index = i - 1;
				if (page_freelists[i - 1]) {
					page_freelists[i - 1]->prev = second;
				}
				page_freelists[i - 1] = second;
			}

			return page;
		}

		return NULL;
	}

	Page* page = page_freelists[index];
	page_freelists[index] = page->next;
	if (page->next) {
		page->next->prev = NULL;
	}
	assert(page->type == PAGE_FREE);
	page->type = PAGE_USED;
	return page;
}

void pmalloc_add_mem(void* base, usize size) {
	// todo
	//kprintf("[kernel][mem]: adding mem 0x%p size %ukb\n", base, size / 1024);

	usize page_count = size / PAGE_SIZE;

	if (page_count == 0) {
		return;
	}

	usize used_pages = (sizeof(PRegion) + page_count * sizeof(Page) + PAGE_SIZE - 1) / PAGE_SIZE;

	PRegion* region = (PRegion*) to_virt((usize) base);
	region->prev = NULL;
	region->next = NULL;
	region->base = (usize) base;
	region->size = page_count;
	region->used_pages = used_pages;
	if (region > pregions_end) {
		region->prev = pregions_end;
		if (pregions_end) {
			pregions_end->next = region;
		}
		else {
			pregions = region;
		}
		pregions_end = region;
	}
	else {
		kprintf("[kernel][mem]: error: regions were not added from lower to higher");
		while (true) {
			arch_hlt();
		}
	}

	for (usize i = 0; i < used_pages; ++i) {
		region->pages[i].prev = NULL;
		region->pages[i].next = NULL;
		region->pages[i].phys = (usize) base + i * PAGE_SIZE;
		region->pages[i].region = region;
		region->pages[i].type = PAGE_USED;
		region->pages[i].list_index = 0;
	}

	usize free_pages = page_count - used_pages;

	if (!free_pages) {
		return;
	}

	for (usize i = used_pages; i < page_count; ++i) {
		region->pages[i].prev = NULL;
		region->pages[i].next = NULL;
		region->pages[i].phys = (usize) base + i * PAGE_SIZE;
		region->pages[i].region = region;
		region->pages[i].type = PAGE_FREE;
		region->pages[i].list_index = 0;
	}

	usize i = used_pages;
	while (i < page_count) {
		usize index = size_to_index(free_pages);
		if (index >= FREELIST_COUNT) {
			index = FREELIST_COUNT - 1;
		}
		usize count = index_to_size(index);

		Page* page = &region->pages[i];
		page->prev = NULL;
		page->next = page_freelists[index];
		page->list_index = index;
		if (page->next) {
			page->next->prev = page;
		}
		page_freelists[index] = page;

		i += count;
		free_pages -= count;
	}
}

static Mutex PMALLOC_LOCK = {};

static Page* pmalloc_memset(usize count, uint8_t value) {
	if (!count) {
		return NULL;
	}
	usize index = size_to_index(count);
	if (index >= FREELIST_COUNT) {
		return NULL;
	}

	if (count & (count - 1)) {
		index += 1;
	}

	mutex_lock(&PMALLOC_LOCK);
	Page* ptr = freelist_get_nonrecursive(index);
	mutex_unlock(&PMALLOC_LOCK);
	if (ptr) {
		memset(to_virt(ptr->phys), value, index_to_size(index) * PAGE_SIZE);
	}
	return ptr;
}

Page* pmalloc(usize count) {
	return pmalloc_memset(count, 0xCB);
}

Page* pcalloc(usize count) {
	return pmalloc_memset(count, 0);
}

void pfree(Page* ptr, usize count) {
	if (!ptr || !count) {
		return;
	}

	usize index = size_to_index(count);
	if (count & (count - 1)) {
		index += 1;
	}

	assert(ptr->type == PAGE_USED && "pfree: double free");
	memset(to_virt(ptr->phys), 0xFE, index_to_size(index) * PAGE_SIZE);
	mutex_lock(&PMALLOC_LOCK);
	freelist_insert_nonrecursive(index, ptr);
	mutex_unlock(&PMALLOC_LOCK);
}