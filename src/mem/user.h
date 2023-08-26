#pragma once
#include "arch/misc.h"

static inline bool mem_copy_to_user(__user void* restrict user, const void* restrict kernel, usize size) {
	return arch_mem_copy_to_user(user, kernel, size);
}

static inline bool mem_copy_to_kernel(void* restrict kernel, __user const void* restrict user, usize size) {
	return arch_mem_copy_to_kernel(kernel, user, size);
}
