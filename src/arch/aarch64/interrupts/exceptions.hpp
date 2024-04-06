#pragma once
#include "types.hpp"

struct ExceptionFrame {
	u64 x0;
	u64 x1;
	u64 x2;
	u64 x3;
	u64 x4;
	u64 x5;
	u64 x6;
	u64 x7;
	u64 x8;
	u64 x9;
	u64 x10;
	u64 x11;
	u64 x12;
	u64 x13;
	u64 x14;
	u64 x15;
	u64 x16;
	u64 x17;
	u64 x18;
	u64 x29;
	u64 x30;
	u64 elr_el1;
	u64 esr_el1;
	u64 far_el1;
};
