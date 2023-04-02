#pragma once
#include "assert.h"
#include "types.h"

#define PAGE_SIZE 0x1000

typedef struct Page {
	struct Page* prev;
	struct Page* next;
	usize phys;
	struct PRegion* region;
	enum : u8 {
		PAGE_FREE,
		PAGE_USED
	} type;
	char pad[7];
} Page;

static_assert(sizeof(Page) == 40);

typedef struct PRegion {
	struct PRegion* prev;
	struct PRegion* next;
	usize base;
	usize size;
	struct Page pages[];
} PRegion;