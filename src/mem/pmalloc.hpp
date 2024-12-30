#pragma once
#include "types.hpp"
#include "double_list.hpp"
#include "utils/spinlock.hpp"

struct PRegion;
struct Page {
	DoubleListHook hook {};
	PRegion* region {};
	usize ref_count {};
	usize page_num : 36 {};
	bool used : 1 {};
	bool in_list : 1 {};
	u8 list_index : 4 {};
	IrqSpinlock<void> lock {};

	[[nodiscard]] constexpr usize phys() const {
		return page_num << 12;
	}

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
usize pmalloc_get_reserved_mem();
usize pmalloc_get_used_mem();
