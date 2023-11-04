#pragma once
#include "types.h"

typedef struct {
	u32 eax;
	u32 ebx;
	u32 ecx;
	u32 edx;
} Cpuid;

Cpuid cpuid(u32 eax, u32 ecx);