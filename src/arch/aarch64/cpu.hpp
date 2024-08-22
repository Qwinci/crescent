#pragma once
#include <stddef.h>

struct CpuFeatures {
	bool pan;
};
static_assert(offsetof(CpuFeatures, pan) == 0);

extern CpuFeatures CPU_FEATURES;
