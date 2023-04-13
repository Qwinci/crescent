#pragma once
#include "types.h"

void udelay(usize us);
void mdelay(usize ms);

#define NS_IN_US 1000
#define US_IN_MS 1000
#define MS_IN_S 1000

void arch_init_timers();
usize arch_get_ns_since_boot();
void arch_create_timer(usize time, void (*fn)());