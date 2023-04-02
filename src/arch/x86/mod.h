#pragma once
#include "types.h"

typedef struct {
	void* base;
	usize size;
} Module;

Module x86_module_get(const char* name);