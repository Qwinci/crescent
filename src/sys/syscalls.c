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
	Task* child = task->children;
	for (; child; child = child->child_next) {
		kprintf("killing child task '%s'", *child->name ? child->name : "<no name>");
		sched_kill_task(child);
	}
	sched_exit();
}

Task* sys_create_thread(void (*fn)(), void* arg) {
	Task* self = arch_get_cur_task();
	Task* task = arch_create_user_task_with_map("", fn, arg, self, self->map);
	if (!task) {
		return NULL;
	}
	task->child_next = self->children;
	self->children = task;
	void* flags = enter_critical();
	sched_queue_task(task);
	leave_critical(flags);
	return task;
}