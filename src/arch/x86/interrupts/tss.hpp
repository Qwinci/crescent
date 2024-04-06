#pragma once
#include "types.hpp"

struct Tss {
	u32 reserved1;
	u32 rsp0_low;
	u32 rsp0_high;
	u32 rsp1_low;
	u32 rsp1_high;
	u32 rsp2_low;
	u32 rsp2_high;
	u32 reserved2[2];
	u32 ist1_low;
	u32 ist1_high;
	u32 ist2_low;
	u32 ist2_high;
	u32 ist3_low;
	u32 ist3_high;
	u32 ist4_low;
	u32 ist4_high;
	u32 ist5_low;
	u32 ist5_high;
	u32 ist6_low;
	u32 ist6_high;
	u32 ist7_low;
	u32 ist7_high;
	u32 reserved3[2];
	u16 reserved4;
	u16 iopb;
};
