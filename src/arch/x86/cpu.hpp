#pragma once
#include "types.hpp"

enum class Msr : u32 {
	Ia32ApicBase = 0x1B,
	Efer = 0xC0000080,
	Star = 0xC0000081,
	LStar = 0xC0000082,
	CStar = 0xC0000083,
	SFMask = 0xC0000084,
	FsBase = 0xC0000100,
	GsBase = 0xC0000101,
	KernelGsBase = 0xC0000102
};

void set_msr(Msr msr, u64 value);
u64 get_msr(Msr msr);