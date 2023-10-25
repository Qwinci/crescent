#pragma once
#include "types.h"

typedef struct {
	u64 fs;
	u64 gs;
} ExecutorGenericState;

typedef struct {
	ExecutorGenericState generic;
} ExecutorState;
