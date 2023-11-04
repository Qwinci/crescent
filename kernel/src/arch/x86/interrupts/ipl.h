#pragma once
#include "types.h"

typedef enum : u64 {
	INT_PRIO_2 = 2, // 32-47
	INT_PRIO_3 = 3, // 48-63
	INT_PRIO_4 = 4, // 64-79
	INT_PRIO_5 = 5, // 80-95
	INT_PRIO_6 = 6, // 96-111
	INT_PRIO_7 = 7, // 112-127
	INT_PRIO_8 = 8, // 128-143
	INT_PRIO_9 = 9, // 144-159
	INT_PRIO_10 = 10, // 160-175
	INT_PRIO_11 = 11, // 176-191
	INT_PRIO_12 = 12, // 192-207
	INT_PRIO_13 = 13, // 208-223
	INT_PRIO_14 = 14, // 224-239
	INT_PRIO_15 = 15 // 240-255
} X86Ipl;

extern X86Ipl X86_IPL_MAP[];