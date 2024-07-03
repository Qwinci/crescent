#include "pmalloc.hpp"
#include "arch/paging.hpp"
#include "bit.hpp"
#include "cstring.hpp"
#include "mem.hpp"
#include "new.hpp"
#ifdef ARCH_USER
#include <assert.h>
#endif

Spinlock<DoubleList<PRegion, &PRegion::hook>> P_REGIONS;

// 1 2 4 8 16 32 64 128 256 512 1024 2048 4096 8192
static constexpr usize FREELIST_COUNT = 14;

namespace {
	Spinlock<DoubleList<Page, &Page::hook>> FREELISTS[FREELIST_COUNT] {};
	usize TOTAL_MEMORY = 0;
}

static constexpr usize index_to_size(usize index) {
	return 1 << index;
}

static constexpr usize size_to_index(usize size) {
	return sizeof(usize) * 8 - kstd::countl_zero(size - 1);
}

static void freelist_insert(usize index, Page* page) {
	page->used = false;
	page->list_index = index;

	usize final_i = index;
	usize new_i = index;
	for (; new_i < FREELIST_COUNT - 1; ++new_i) {
		usize count = index_to_size(new_i);
		usize size = count * PAGE_SIZE;

		usize region_free_base = page->region->base + page->region->res_count * PAGE_SIZE;
		usize page_offset = page->phys() - region_free_base;
		usize next_size = index_to_size(new_i + 1) * PAGE_SIZE;
		usize buddy_phys = (page_offset & (next_size - 1)) ? page->phys() - size : page->phys() + size;
		usize buddy_index = (buddy_phys - page->region->base) / PAGE_SIZE;
		if (buddy_index >= page->region->page_count) {
			break;
		}

		auto* buddy = &page->region->pages[buddy_index];

		IrqGuard irq_guard {};
		auto guard = buddy->lock.lock();

		if (!buddy->used && buddy->list_index == page->list_index) {
			if (buddy->in_list) {
				FREELISTS[buddy->list_index].lock()->remove(buddy);
				buddy->in_list = false;
			}

			auto* first = page < buddy ? page : buddy;
			first->in_list = false;
			first->list_index = new_i + 1;

			page = first;
			final_i = new_i + 1;
		}
		else {
			break;
		}
	}

	IrqGuard irq_guard {};
	auto guard = page->lock.lock();
	page->in_list = true;
	FREELISTS[final_i].lock()->push(page);
}

static Page* freelist_get(usize index) {
	{
		IrqGuard irq_guard {};
		auto guard = FREELISTS[index].lock();

		if (!guard->is_empty()) {
			auto page = guard->pop();
			page->used = true;
			page->in_list = false;
			return page;
		}
	}

	if (index == FREELIST_COUNT - 1) {
		return nullptr;
	}

	usize new_i = index + 1;
	for (; new_i < FREELIST_COUNT; ++new_i) {
		Page* page;
		{
			IrqGuard irq_guard {};
			auto other_guard = FREELISTS[new_i].lock();
			if (other_guard->is_empty()) {
				continue;
			}
			page = other_guard->pop();
			page->in_list = false;
			page->used = true;
			page->list_index = index;
		}

		auto* region = page->region;
		usize page_num = (page->phys() - region->base) / PAGE_SIZE;

		for (usize i = new_i; i > index; --i) {
			usize size = index_to_size(i);

			auto* second = &region->pages[page_num + size / 2];

			second->list_index = i - 1;

			IrqGuard irq_guard {};
			auto other_guard = FREELISTS[i - 1].lock();
			other_guard->push(second);
			second->in_list = true;
		}

		return page;
	}

	return nullptr;
}

void pmalloc_add_mem(usize phys, usize size) {
	usize pages = size / PAGE_SIZE;
	if (pages == 0) {
		return;
	}

	usize needed = sizeof(PRegion) + (pages - 1) * sizeof(Page);
	if (needed >= size) {
		return;
	}
	usize res_pages = ALIGNUP(needed, PAGE_SIZE) / PAGE_SIZE;

	TOTAL_MEMORY += size;

	auto* region = new (to_virt<void>(phys)) PRegion {
		.hook {},
		.base = phys,
		.page_count = pages,
		.res_count = res_pages,
		.pages {}
	};

	for (usize i = 0; i < res_pages; ++i) {
		new (&region->pages[i]) Page {
			.region = region,
			.page_num = (phys >> 12) + i
		};
	}

	for (usize i = res_pages; i < pages; ++i) {
		new (&region->pages[i]) Page {
			.region = region,
			.page_num = (phys >> 12) + i,
			.list_index = FREELIST_COUNT
		};
	}

	usize i = res_pages;
	usize free_pages = pages - res_pages;
	while (i < pages) {
		auto& page = region->pages[i];

		for (usize j = FREELIST_COUNT; j > 0; --j) {
			usize block_pages = index_to_size(j - 1);
			if (page.phys() % (block_pages * PAGE_SIZE) == 0 && free_pages >= block_pages) {
				IrqGuard irq_guard {};
				auto guard = FREELISTS[j - 1].lock();
				guard->push(&page);
				page.list_index = j - 1;
				page.in_list = true;

				i += block_pages;
				free_pages -= block_pages;
				break;
			}
		}
	}

	IrqGuard irq_guard {};
	auto guard = P_REGIONS.lock();
	if (!guard->is_empty()) {
		auto reg = guard->find([&](PRegion& reg) {
			return &reg > region;
		});
		guard->insert(reg, region);
	}
	else {
		guard->push(region);
	}
}

usize pmalloc(usize count) {
	if (!count) {
		return 0;
	}

	auto page = freelist_get(size_to_index(count));
	if (page) {
		memset(to_virt<void>(page->phys()), 0xCB, count * PAGE_SIZE);
		return page->phys();
	}
	else {
		return 0;
	}
}

void pfree(usize addr, usize count) {
	auto page = Page::from_phys(addr);
	assert(page->phys() == addr);
	assert(page->used);
	assert(page->list_index == size_to_index(count));

	memset(to_virt<void>(addr), 0xFD, count * PAGE_SIZE);
	freelist_insert(page->list_index, page);
}

Page* Page::from_phys(usize phys) {
	IrqGuard irq_guard {};
	auto guard = P_REGIONS.lock();

	for (auto& reg : *guard) {
		if (phys >= reg.base && phys < reg.base + reg.page_count * PAGE_SIZE) {
			return &reg.pages[(phys - reg.base) / PAGE_SIZE];
		}
	}

	return nullptr;
}

usize pmalloc_get_total_mem() {
	return TOTAL_MEMORY;
}
