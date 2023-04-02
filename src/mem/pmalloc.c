#include "pmalloc.h"
#include "arch/misc.h"
#include "page.h"
#include "stdio.h"
#include "utils.h"

// 1 2 4 8 16 32 64 128 256 512 1024
#define FREELIST_COUNT 11

PRegion* pregions = NULL;
PRegion* pregions_end = NULL;

static Page* freelists[FREELIST_COUNT] = {};

static usize index_to_size(usize index) {
	return 1 << index;
}

static usize size_to_index(usize size) {
	return sizeof(unsigned long long) * 8 - __builtin_clzll(size) - 1;
}

static void freelist_insert_nonrecursive(usize index, Page* page) {
	page->type = PAGE_FREE;
	page->prev = NULL;

	usize new_i = index;
	while (new_i < FREELIST_COUNT) {
		usize count = index_to_size(new_i);
		usize size = count * PAGE_SIZE;

		usize buddy = (page->phys - page->region->base) & (index_to_size(new_i + 1) * PAGE_SIZE) ? page->phys - size : page->phys + size;
		usize buddy_index = (buddy - page->region->base) / PAGE_SIZE;
		if (!freelists[new_i] || new_i + 1 == FREELIST_COUNT || buddy_index >= page->region->size) {
			break;
		}

		Page* buddy_page = &page->region->pages[buddy_index];
		if (buddy_page->type == PAGE_FREE) {
			if (buddy_page->prev) {
				buddy_page->prev->next = buddy_page->next;
			}
			else {
				freelists[new_i] = buddy_page->next;
			}
			if (buddy_page->next) {
				buddy_page->next->prev = buddy_page->prev;
			}

			Page* first_page = page < buddy_page ? page : buddy_page;

			first_page->prev = NULL;
			first_page->next = freelists[new_i + 1];
			if (freelists[new_i + 1]) {
				freelists[new_i + 1]->prev = first_page;
			}
			freelists[new_i + 1] = first_page;

			page = first_page;
		}
		else {
			break;
		}

		new_i += 1;
	}

	if (new_i == index) {
		page->prev = NULL;
		page->next = freelists[index];
		if (freelists[index]) {
			freelists[index]->prev = page;
		}
		freelists[index] = page;
	}
}

static void freelist_insert(usize index, Page* page) { // NOLINT(misc-no-recursion)
	page->type = PAGE_FREE;
	page->prev = NULL;

	usize size = index_to_size(index) * PAGE_SIZE;
	usize buddy = (page->phys - page->region->base) & (index_to_size(index + 1)) ? page->phys - size : page->phys + size;
	usize buddy_index = (buddy - page->region->base) / PAGE_SIZE;
	if (freelists[index] && index + 1 < FREELIST_COUNT && buddy_index < page->region->size) {
		Page* buddy_page = &page->region->pages[buddy_index];
		if (buddy_page->type == PAGE_FREE) {
			if (buddy_page->prev) {
				buddy_page->prev->next = buddy_page->next;
			}
			else {
				freelists[index] = buddy_page->next;
			}
			if (buddy_page->next) {
				buddy_page->next->prev = buddy_page->prev;
			}

			Page* first_page = page < buddy_page ? page : buddy_page;
			freelist_insert(index + 1, first_page);
			return;
		}
	}

	if (!freelists[index]) {
		page->next = NULL;
		freelists[index] = page;
	}
	else {
		page->next = freelists[index];
		freelists[index]->prev = page;
		freelists[index] = page;
	}
}

static Page* freelist_get_nonrecursive(usize index) {
	if (!freelists[index]) {
		if (index + 1 == FREELIST_COUNT) {
			return NULL;
		}

		usize new_i = index + 1;
		while (new_i < FREELIST_COUNT) {
			if (!freelists[new_i]) {
				new_i += 1;
				continue;
			}

			Page* page = freelists[new_i];
			freelists[new_i] = page->next;
			if (page->next) {
				page->next->prev = NULL;
			}
			page->type = PAGE_USED;

			PRegion* region = page->region;

			for (usize i = new_i; i > index; --i) {
				usize size = index_to_size(i);

				usize page_num = (page->phys - region->base) / PAGE_SIZE;
				Page* second = &region->pages[page_num + size / 2];

				second->prev = NULL;
				second->next = freelists[i - 1];
				if (freelists[i - 1]) {
					freelists[i - 1]->prev = second;
				}
				freelists[i - 1] = second;
			}

			return page;
		}

		return NULL;
	}

	Page* page = freelists[index];
	freelists[index] = page->next;
	if (page->next) {
		page->next->prev = NULL;
	}
	page->type = PAGE_USED;
	return page;
}

static Page* freelist_get(usize index) { // NOLINT(misc-no-recursion)
	if (!freelists[index]) {
		usize size = index_to_size(index);
		if (index < FREELIST_COUNT - 2) {
			Page* first_half = freelist_get(index + 1);
			if (!first_half) {
				return NULL;
			}

			PRegion* region = first_half->region;
			usize first_page_num = (first_half->phys - region->base) / PAGE_SIZE;
			Page* second_half = &region->pages[first_page_num + size / 2];

			first_half->type = PAGE_USED;
			freelist_insert(index, second_half);
			return first_half;
		}
		else {
			return NULL;
		}
	}

	Page* page = freelists[index];
	freelists[index] = page->next;
	if (page->next) {
		page->next->prev = NULL;
	}
	page->type = PAGE_USED;
	return page;
}

void pmalloc_add_mem(void* base, usize size) {
	kprintf("[kernel][mem]: adding mem 0x%p size %ukb\n", base, size / 1024);

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
	if (region > pregions_end) {
		region->prev = pregions_end;
		if (pregions_end) {
			pregions_end->next = region;
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
		region->pages[i].phys = (usize) base + i * PAGE_SIZE;
		region->pages[i].region = region;
		region->pages[i].type = PAGE_USED;
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
	}

	region->pages[used_pages - 1].next = NULL;
	region->pages[0].prev = NULL;

	usize i = used_pages;
	while (i < page_count) {
		usize index = size_to_index(free_pages);
		if (index >= FREELIST_COUNT) {
			index = FREELIST_COUNT - 1;
		}
		usize count = index_to_size(index);

		//freelist_insert(index, &region->pages[i]);
		freelist_insert_nonrecursive(index, &region->pages[i]);

		i += count;
		free_pages -= count;
	}
}

Page* pmalloc(usize count) {
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

	//return freelist_get(index);
	return freelist_get_nonrecursive(index);
}

void pfree(Page* ptr, usize count) {
	usize index = size_to_index(count);
	//freelist_insert(index, ptr);
	freelist_insert_nonrecursive(index, ptr);
}