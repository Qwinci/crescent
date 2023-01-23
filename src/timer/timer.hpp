#pragma once
#include "types.hpp"

static inline void udelay(u64 us) {
	extern void (*udelay_ptr)(u64 us);
	udelay_ptr(us);
}


void init_timers(const void* rsdp);