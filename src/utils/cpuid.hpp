#pragma once
#include "types.hpp"

struct Cpuid {
	u32 eax;
	u32 ebx;
	u32 ecx;
	u32 edx;
};

static inline Cpuid cpuid(u32 eax, u32 ecx = 0) {
	Cpuid id {};
	asm volatile("cpuid" : "=a"(id.eax), "=b"(id.ebx), "=c"(id.ecx), "=d"(id.edx) : "a"(eax), "c"(ecx));
	return id;
}