#pragma once
#include "types.hpp"

bool initialize_hpet(const void* hpet_ptr);
void hpet_sleep(u64 us);
u64 hpet_get_current();
u64 hpet_diff_to_ns(u64 diff);
bool hpet_reselect_irq();