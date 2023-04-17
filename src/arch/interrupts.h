#pragma once
#include "types.h"

typedef bool (*IntHandler)(void* ctx, void* userdata);

typedef struct {
	IntHandler handler;
	void* userdata;
} HandlerData;

u32 arch_alloc_int(IntHandler handler, void* userdata);
HandlerData arch_set_handler(u32 i, IntHandler handler, void* userdata);
void arch_dealloc_int(u32 i);

typedef enum : u64 {
	INT_PRIO_2 = 2,
	INT_PRIO_3 = 3,
	INT_PRIO_4 = 4,
	INT_PRIO_5 = 5,
	INT_PRIO_6 = 6,
	INT_PRIO_7 = 7,
	INT_PRIO_8 = 8,
	INT_PRIO_9 = 9,
	INT_PRIO_10 = 10,
	INT_PRIO_11 = 11,
	INT_PRIO_12 = 12,
	INT_PRIO_13 = 13,
	INT_PRIO_14 = 14,
	INT_PRIO_15 = 15
} Ipl;