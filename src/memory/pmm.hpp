#pragma once
#include "types.hpp"

struct Page {
	usize phys;
	enum class Queue : u8 {
		None,
		Free
	} queue;
	u8 reserved[55];
};

static_assert(sizeof(Page) == 64);

struct PRegion {
	PRegion* next;
	usize base;
	usize p_count;
	Page pages[];
};

#define ALIGNUP(Num, Align) (((Num) + (Align - 1)) & ~(Align - 1))
#define ALIGNDOWN(Num, Align) ((Num) & ~(Align - 1))