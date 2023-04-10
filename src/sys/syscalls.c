#include "arch/cpu.h"
#include "arch/misc.h"
#include "sched/sched.h"
#include "stdio.h"
#include "types.h"

usize syscall_handler_count = 2;

void sys_exit(int status);
Task* sys_create_thread(void (*fn)(), void* arg);

void* syscall_handlers[] = {
	sys_exit,
	sys_create_thread
};

void sys_exit(int status) {
	Task* task = arch_get_cur_task();
	kprintf("task '%s' exited with status %d\n", task->name, status);
	sched_exit();
}

Task* sys_create_thread(void (*fn)(), void* arg) {
	Task* task = arch_create_user_task("", fn, arg, arch_get_cur_task());
	if (!task) {
		return NULL;
	}
	Task* self = arch_get_cur_task();
	void* flags = enter_critical();
	sched_queue_task(task);
	leave_critical(flags);
	return task;
}