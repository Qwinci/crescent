#pragma once
#include "types.hpp"

struct Page {
	usize phys;
	Page* next;
	enum class Queue : u8 {
		None,
		Free,
		Dma
	} queue;
	u8 reserved[47];
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