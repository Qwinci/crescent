#pragma once
#include "str.h"

typedef struct {
	Str init;
	Str posix_root;
} KernelCmdline;

KernelCmdline parse_kernel_cmdline(const char* cmdline);
