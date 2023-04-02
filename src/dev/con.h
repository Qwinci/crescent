#pragma once
#include "types.h"

typedef struct Con {
	int (*write)(struct Con* self, char c);
	int (*write_at)(struct Con* self, usize x, usize y, char c);
	usize column;
	usize line;
	u32 fg;
	u32 bg;
} Con;

extern Con* kernel_con;