#include "arch/cpu.h"
#include "arch/misc.h"
#include "sched/sched.h"
#include "stdio.h"
#include "types.h"

__attribute__((used)) usize syscall_handler_count = 5;

void sys_exit(int status);
Task* sys_create_thread(void (*fn)(), void* arg, bool detach);
void sys_dprint(const char* msg, size_t len);
void sys_sleep(usize ms);
int sys_wait_thread(Task* thread);

__attribute__((used)) void* syscall_handlers[] = {
	sys_exit,
	sys_create_thread,
	sys_dprint,
	sys_sleep,
	sys_wait_thread
};

#define E_DETACHED (-1)

int sys_wait_thread(Task* thread) {
	if (thread->detached) {
		return E_DETACHED;
	}

	sched_sigwait(thread);
	int status = thread->status;
	thread->detached = true;
	return status;
}

void sys_sleep(usize ms) {
	sched_sleep(ms * US_IN_MS);
}

void sys_dprint(const char* msg, size_t len) {
	kputs(msg, len);
}

void sys_exit(int status) {
	Task* task = arch_get_cur_task();
	kprintf("task '%s' exited with status %d\n", task->name, status);
	sched_exit(status);
}

Task* sys_create_thread(void (*fn)(), void* arg, bool detach) {
	Task* self = arch_get_cur_task();
	Task* task = arch_create_user_task_with_map("user thread", fn, arg, self, self->map, self->user_vmem, detach);
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