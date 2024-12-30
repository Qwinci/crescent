#pragma once
#include "vm.hpp"

void vga_init(Vm* vm);
void vga_print_text_mem(uint32_t* fb);
