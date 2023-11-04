#pragma once
#include "str.h"

typedef struct {
	Str init;
} KernelCmdline;

KernelCmdline parse_kernel_cmdline(const char* cmdline);
