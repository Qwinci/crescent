#pragma once
#include "types.h"

void arch_hlt();
void arch_spinloop_hint();

void arch_reboot();

#if defined(__clang__)
#define __user __attribute__((address_space(1), noderef))
#else
#define __user
#endif

bool arch_mem_copy_to_user(__user void* restrict user, const void* restrict kernel, usize size);
bool arch_mem_copy_to_kernel(void* restrict kernel, __user const void* restrict user, usize size);
