#pragma once
#include "arch/cpu.h"

static inline void start_catch_faults() {
	arch_get_cur_task()->inside_syscall = true;
}

static inline void end_catch_faults() {
	arch_get_cur_task()->inside_syscall = false;
}