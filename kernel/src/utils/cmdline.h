#pragma once
#include "str.h"

typedef struct {
	Str init;
	Str posix_root;
	bool native_init;
} KernelCmdline;

KernelCmdline parse_kernel_cmdline(const char* cmdline);
