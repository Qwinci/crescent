#include "page_allocator.hpp"
#include "new.hpp"
#include "algorithm.hpp"

PRegion* PREGIONS_START;
PRegion* PREGIONS_END;

static constexpr u8 size_to_index(usize size) {
	if (size == 1) {
		return 0;
	}
	else {
		return static_cast<u8>((sizeof(unsigned long long) * 8) - __builtin_clzll(size - 1));
	}
}

static constexpr usize index_to_size(u8 index) {
	return 1 << index;
}

void PageAllocator::add_mem(usize phys, void* virt, usize size) {
	usize pages = size / PAGE_SIZE;

	if (pages == 0) {
		return;
	}

	usize used_pages = ALIGNUP(sizeof(PRegion) + pages * sizeof(Page), PAGE_SIZE) / PAGE_SIZE;

	auto region = new (virt) PRegion {
		.phys_base = phys,
		.page_count = pages,
		.used_pages = used_pages
	};

	if (!PREGIONS_START) {
		PREGIONS_START = region;
		PREGIONS_END = region;
	}
	else if (region < PREGIONS_START) {
		PREGIONS_START->prev = region;
		region->next = PREGIONS_START;
		PREGIONS_START = region;
	}
	else if (region > PREGIONS_END) {
		PREGIONS_END->next = region;
		region->prev = PREGIONS_END;
		PREGIONS_END = region;
	}
	else {
		for (PRegion* reg = PREGIONS_START;; reg = reg->next) {
			if (reg->next > region) {
				region->next = reg->next;
				reg->next->prev = region;
				reg->next = region;
				break;
			}
		}
	}

	for (usize i = 0; i < pages; ++i) {
		new (&region->pages[i]) Page {
			.region = region,
			.phys = phys + i * PAGE_SIZE
		};
	}

	for (usize i = 0; i < used_pages; ++i) {
		region->pages[i].list = Page::LIST_USED;
	}

	usize free_pages = pages - used_pages;

	for (usize i = used_pages; free_pages;) {
		u8 list_i = size_to_index(free_pages);

		if (list_i >= FREELIST_COUNT) {
			list_i = FREELIST_COUNT - 1;
		}
		else if (free_pages & (free_pages - 1)) {
			list_i -= 1;
		}

		usize list_size = index_to_size(list_i);

		auto page = &region->pages[i];
		page->list = list_i;
		page->next = freelists[list_i];
		if (page->next) {
			page->next->prev = page;
		}
		freelists[list_i] = page;

		i += list_size;
		free_pages -= list_size;
	}
}

void PageAllocator::remove_first_entry(Page* entry) {
	freelists[entry->list] = entry->next;
	if (entry->next) {
		entry->next->prev = nullptr;
	}
	entry->list = Page::LIST_USED;
}

void PageAllocator::remove_entry(Page* entry) {
	if (entry->prev) {
		entry->prev->next = entry->next;
	}
	else {
		freelists[entry->list] = entry->next;
	}
	if (entry->next) {
		entry->next->prev = entry->prev;
	}
	entry->list = Page::LIST_USED;
}

void PageAllocator::insert_entry(u8 list, Page* entry) {
	entry->list = list;
	entry->next = freelists[list];
	if (entry->next) {
		entry->next->prev = entry;
	}
	freelists[list] = entry;
}

Page* PageAllocator::alloc(usize count) {
	u8 list_i = size_to_index(count);
	if (count & (count - 1)) {
		list_i += 1;
	}
	if (list_i >= FREELIST_COUNT) {
		return nullptr;
	}

	if (freelists[list_i]) {
		auto entry = freelists[list_i];
		remove_first_entry(entry);
		return entry;
	}

	u8 orig_i = list_i;
	list_i += 1;
	for (; list_i < FREELIST_COUNT; ++list_i) {
		auto entry = freelists[list_i];
		if (!entry) {
			continue;
		}

		remove_first_entry(entry);
		for (usize i = list_i; i > orig_i; --i) {
			usize list_size = index_to_size(i - 1);
			Page* second = &entry->region->pages[(entry->phys - entry->region->phys_base) / PAGE_SIZE + list_size];
			insert_entry(i - 1, second);
		}
		return entry;
	}

	return nullptr;
}

void PageAllocator::merge_entries(u8 list, Page* entry) {
	usize free_region_start = entry->region->phys_base + entry->region->used_pages * PAGE_SIZE;
	while (true) {
		usize list_size = index_to_size(list) * PAGE_SIZE;
		usize entry_off = entry->phys - free_region_start;
		usize buddy_off = entry_off ^ list_size;
		usize buddy_phys = free_region_start + buddy_off;

		if (list == FREELIST_COUNT - 1 || !entry->region->contains(buddy_phys)) {
			insert_entry(list, entry);
			return;
		}
		Page* buddy = &entry->region->pages[(buddy_phys - entry->region->phys_base) / PAGE_SIZE];
		if (buddy->list != list) {
			insert_entry(list, entry);
			return;
		}

		remove_entry(buddy);
		Page* first = min(entry, buddy);
		list += 1;
		entry = first;
	}
}

void PageAllocator::free(Page* entry, usize count) {
	u8 list_i = size_to_index(count);
	if (count & (count - 1)) {
		list_i += 1;
	}

	merge_entries(list_i, entry);
}

PageAllocator PAGE_ALLOCATOR {};
