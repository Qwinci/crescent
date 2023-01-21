#pragma once
#include "types.hpp"

static inline void udelay(u64 us) {
	extern void (*udelay_ptr)(u64 us);
	udelay_ptr(us);
}

u64 get_current_timer_ns();
void start_lapic_timer();

void init_timers(const void* rsdp);