#pragma once
#include "types.h"

typedef bool (*IntHandler)(void* ctx, void* userdata);

typedef struct {
	IntHandler handler;
	void* userdata;
} HandlerData;

u32 arch_alloc_int(IntHandler handler, void* userdata);
HandlerData arch_set_handler(u32 i, IntHandler handler, void* userdata);
void arch_dealloc_int(u32 i);