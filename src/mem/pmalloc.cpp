#include "pmalloc.hpp"
#include "arch/paging.hpp"
#include "mem.hpp"
#include "new.hpp"
#include "cstring.hpp"
#ifdef ARCH_USER
#include <assert.h>
#endif

Spinlock<DoubleList<PRegion, &PRegion::hook>> P_REGIONS;

namespace {
	Spinlock<DoubleList<Page, &Page::hook>> FREELIST;
	usize TOTAL_MEMORY = 0;
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
		auto& page = region->pages[i];
		page.region = region;
		page.phys = phys + i * PAGE_SIZE;
	}

	{
		auto freelist_guard = FREELIST.lock();
		for (usize i = res_pages; i < pages; ++i) {
			auto& page = region->pages[i];
			page.region = region;
			page.phys = phys + i * PAGE_SIZE;
			freelist_guard->push(&page);
		}
	}

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

usize pmalloc(usize) {
	auto guard = FREELIST.lock();
	if (auto page = guard->pop()) {
		memset(to_virt<void>(page->phys), 0xCB, PAGE_SIZE);
		return page->phys;
	}
	else {
		return 0;
	}
}

void pfree(usize addr, usize) {
	auto page = Page::from_phys(addr);
	assert(page->phys == addr);

	memset(to_virt<void>(addr), 0xFD, PAGE_SIZE);
	auto guard = FREELIST.lock();
	guard->push(page);
}

Page* Page::from_phys(usize phys) {
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
