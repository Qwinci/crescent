#include "console.hpp"
#include "map.hpp"
#include "memory.hpp"
#include "new.hpp"
#include "pmm.hpp"
#include "std.hpp"
#include "utils.hpp"
#include "utils/math.hpp"

struct PRegionList {
	PRegion* root {};
	PRegion* end {};
};

PageAllocator PAGE_ALLOCATOR;
PRegionList region_list {};
Spinlock alloc_lock;

static bool dma_allocated = false;
constexpr usize DMA_RESERVED = 1024 * 1024 * 100;

void PageAllocator::add_mem(usize base, usize size) {
	auto virt = PhysAddr {base}.to_virt();

	auto p_count = size / PAGE_SIZE;

	auto region = new (virt) PRegion;
	region->base = base;
	region->p_count = p_count;
	region->next = nullptr;

	if (!region_list.end) {
		region_list.root = region;
		region_list.end = region;
	}
	else {
		region_list.end->next = region;
		region_list.end = region;
	}

	auto used = ALIGNUP(sizeof(PRegion) + p_count * sizeof(Page), PAGE_SIZE);
	auto used_pages = used / PAGE_SIZE;

	for (usize i = 0; i < used_pages; ++i) {
		auto& page = region->pages[i];
		page.phys = base + i * PAGE_SIZE;
		page.queue = Page::Queue::None;
		page.mappings = nullptr;
	}

	usize dma_used_pages = 0;

	if (!dma_allocated && size - used >= DMA_RESERVED) {
		dma_used_pages = DMA_RESERVED / PAGE_SIZE;
		auto dma_freelist_end = &dma_freelist;
		for (usize i = used_pages; i < used_pages + dma_used_pages; ++i) {
			auto& page = region->pages[i];
			page.phys = base + i * PAGE_SIZE;
			page.queue = Page::Queue::Dma;
			page.next = nullptr;
			page.mappings = nullptr;
			*dma_freelist_end = &page;
			dma_freelist_end = &page.next;
		}
		dma_allocated = true;
	}

	for (usize i = used_pages + dma_used_pages; i < p_count; ++i) {
		auto& page = region->pages[i];
		page.phys = base + i * PAGE_SIZE;
		page.queue = Page::Queue::Free;
		page.mappings = nullptr;
	}

	for (usize i = used + dma_used_pages; i < size; i += PAGE_SIZE) {
		auto node = new (virt.offset(i)) Node;
		node->next = root;
		root = node;
	}
}

void* PageAllocator::alloc_new(bool lock, bool persistent) {
	if (lock) {
		alloc_lock.lock();
	}

	if (!persistent && dma_freelist) {
		auto node = dma_freelist;
		dma_freelist = node->next;

		auto hash = murmur64(node->phys);

		auto& list = dma_move_hashtable[hash % DMA_HASHTABLE_SIZE];
		node->next = list;
		list = node;

		alloc_lock.unlock();
		return cast<void*>(node->phys);
	}

	if (!root) {
		return nullptr;
	}

	auto node = root;
	root = root->next;

	if (lock) {
		alloc_lock.unlock();
	}

	return cast<void*>(VirtAddr {node}.to_phys().as_usize());
}

void PageAllocator::dealloc_new(void* ptr, bool lock) {
	auto node = new (PhysAddr {ptr}.to_virt()) Node;

	if (lock) {
		alloc_lock.lock();
	}

	node->next = root;
	root = node;

	if (lock) {
		alloc_lock.unlock();
	}
}

void* PageAllocator::alloc_dma_helper(Page*& page_list, usize count) {
	Page* prev = nullptr;
	auto start = page_list;
	while (start) {
		auto page = start;
		bool found = true;
		for (usize i = 0; i < count; ++i) {
			if (!page->next || page->next->phys != page->phys + PAGE_SIZE) {
				auto list = dma_move_hashtable[murmur64(page->phys + PAGE_SIZE) % DMA_HASHTABLE_SIZE];
				for (; list; list = list->next) {
					if (list->phys == page->phys + PAGE_SIZE) {
						for (auto mapping = list->mappings; mapping; mapping = mapping->next) {
							auto new_page = PAGE_ALLOCATOR.alloc_new(false);
							if (!new_page) {
								list = nullptr;
								break;
							}
							memcpy(PhysAddr {new_page}.to_virt(), PhysAddr {list->phys}.to_virt(), PAGE_SIZE);
							mapping->map->map(VirtAddr {mapping->virt}, PhysAddr {new_page}, PageFlags::Existing);

							if (mapping->map == get_map()) {
								mapping->map->refresh_page(mapping->virt);
							}
						}
						break;
					}
				}

				if (!list) {
					found = false;
					break;
				}
			}
			page = page->next;
		}

		if (found) {
			if (!prev) {
				page_list = page;
			}
			else {
				prev->next = page;
			}
			return cast<void*>(start->phys);
		}

		prev = start;
		start = start->next;
	}

	return nullptr;
}

void* PageAllocator::alloc_dma(usize count) {
	alloc_lock.lock();

	if (!dma_freelist) {
		for (auto& list : dma_move_hashtable) {
			auto result = alloc_dma_helper(list, count);
			if (result) {
				alloc_lock.unlock();
				return result;
			}
		}

		alloc_lock.unlock();
		return nullptr;
	}

	auto result = alloc_dma_helper(dma_freelist, count);

	alloc_lock.unlock();

	return result;
}

Page* phys_to_page(usize phys) {
	for (auto region = region_list.root; region; region = region->next) {
		if (phys >= region->base && phys <= region->base + region->p_count * PAGE_SIZE) {
			return &region->pages[(phys - region->base) / PAGE_SIZE];
		}
	}
	return nullptr;
}

void PageAllocator::dealloc_dma(void* ptr, usize count) {
	alloc_lock.lock();

	auto page = phys_to_page(cast<usize>(ptr));
	Page* end = page;
	for (usize i = 0; i < count - 1; ++i) {
		end->next = end + 1;
		end = end->next;
	}
	end->next = nullptr;

	if (!dma_freelist) {
		dma_freelist = page;
		alloc_lock.unlock();
		return;
	}

	if (dma_freelist->phys > end->phys) {
		end->next = dma_freelist;
		dma_freelist = page;
		alloc_lock.unlock();
		return;
	}

	auto node = dma_freelist;
	while (node->next) {
		if (node->next->phys > end->phys) {
			break;
		}
	}

	end->next = node->next;
	node->next = page;

	alloc_lock.unlock();
}
