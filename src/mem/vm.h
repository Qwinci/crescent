#pragma once
#include "types.h"

void vm_kernel_init(usize base, usize size);
void* vm_kernel_alloc(usize count);
void vm_kernel_dealloc(void* ptr, usize count);