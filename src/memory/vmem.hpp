#pragma once
#include "types.hpp"

void vm_kernel_init(usize base, usize size);
void* vm_kernel_alloc(usize count);
void vm_kernel_dealloc(void* ptr, usize count);
void* vm_kernel_alloc_backed(usize count);
void vm_kernel_dealloc_backed(void* ptr, usize count);