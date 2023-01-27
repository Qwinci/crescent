#pragma once
#include "types.hpp"

static inline void udelay(u64 us) {
	extern void (*udelay_ptr)(u64 us);
	udelay_ptr(us);
}

extern u16 timer_vec;

void init_timers(const void* rsdp);