#include "types.h"
#include "sched/sched.h"

usize syscall_handler_count = 1;

void sys_exit() {
	sched_exit();
}

void* syscall_handlers[] = {
	sys_exit
};