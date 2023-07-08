#pragma once
#include "types.hpp"

struct Page {
	Page* prev;
	Page* next;
	struct PRegion* region;
	usize phys;
	u8 list;

	static Page* from_phys(usize phys);

	static constexpr u8 LIST_USED = ~0;
};

struct PRegion {
	[[nodiscard]] constexpr bool contains(usize phys) const {
		return phys >= phys_base && phys < phys_base + page_count * PAGE_SIZE;
	}

	struct PRegion* prev;
	struct PRegion* next;
	usize phys_base;
	usize page_count;
	usize used_pages;
	Page pages[];
};
