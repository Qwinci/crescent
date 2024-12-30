#pragma once
#include "vm.hpp"

void chipset_init(Vm* vm, uint32_t* fb, uint32_t fb_size);
void chipset_update_timers(uint64_t elapsed_tsc, const ArchInfo& info);
