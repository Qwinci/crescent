#include "utils.h"
#include "arch/cpu.h"

void syscall_begin() {
	arch_get_cur_task()->inside_syscall = true;
}

void syscall_end() {
	arch_get_cur_task()->inside_syscall = false;
}