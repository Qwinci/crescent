#pragma once
#include "types.h"

void udelay(usize us);
void mdelay(usize ms);

void arch_init_timers();
usize arch_get_ns_since_boot();
void arch_create_timer(usize time, void (*fn)());