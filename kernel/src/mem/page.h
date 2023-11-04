#pragma once
#include "assert.h"
#include "types.h"

#define PAGE_SIZE 0x1000

typedef struct Page {
	struct Page* prev;
	struct Page* next;
	usize phys;
	struct PRegion* region;
	u32 refs;
	enum : u8 {
		PAGE_FREE,
		PAGE_USED
	} type;
	u8 list_index;
	union {
		char pad[2];
	};
} Page;

static_assert(sizeof(Page) == 40);

typedef struct PRegion {
	struct PRegion* prev;
	struct PRegion* next;
	usize base;
	usize size;
	u64 used_pages;
	struct Page pages[];
} PRegion;

static_assert(sizeof(PRegion) == 40);

Page* page_from_addr(usize addr);