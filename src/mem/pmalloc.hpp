#pragma once
#include "types.hpp"
#include "double_list.hpp"
#include "utils/spinlock.hpp"

struct PRegion;
struct Page {
	DoubleListHook hook {};
	PRegion* region {};
	usize phys {};

	static Page* from_phys(usize phys);
};

struct PRegion {
	DoubleListHook hook {};
	usize base {};
	usize page_count {};
	usize res_count {};
	Page pages[1];
};

extern Spinlock<DoubleList<PRegion, &PRegion::hook>> P_REGIONS;

void pmalloc_add_mem(usize phys, usize size);
usize pmalloc(usize count);
void pfree(usize addr, usize count);
usize pmalloc_get_total_mem();
