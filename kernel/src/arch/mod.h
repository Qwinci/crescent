#pragma once
#include "types.h"

typedef struct {
	void* base;
	usize size;
} Module;

Module arch_get_module(const char* name);
const char* arch_get_kernel_cmdline();
