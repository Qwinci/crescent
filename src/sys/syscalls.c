#include "arch/cpu.h"
#include "arch/interrupts.h"
#include "arch/misc.h"
#include "mem/utils.h"
#include "sched/sched.h"
#include "stdio.h"
#include "types.h"
#include "utils/handle.h"
#include "mem/allocator.h"
#include "assert.h"

void sys_exit(int status);
HandleId sys_create_thread(void (*fn)(), void* arg, bool detach);
void sys_dprint(const char* msg, size_t len);
void sys_sleep(usize ms);
int sys_wait_thread(HandleId handle);
void sys_wait_for_event(Event* res);
bool sys_poll_event(Event* res);
bool sys_shutdown(ShutdownType type);

__attribute__((used)) void* syscall_handlers[] = {
	sys_exit,
	sys_create_thread,
	sys_dprint,
	sys_sleep,
	sys_wait_thread,
	sys_wait_for_event,
	sys_poll_event,
	sys_shutdown
};

__attribute__((used)) usize syscall_handler_count = sizeof(syscall_handlers) / sizeof(*syscall_handlers);

#define E_DETACHED (-1)
#define E_ARG (-2)

typedef struct {
	Handle common;
	union {
		Task* task;
		int status;
	};
	bool exited;
} ThreadHandle;

bool sys_shutdown(ShutdownType type) {
	if (type == SHUTDOWN_TYPE_REBOOT) {
		arch_reboot();
	}
	return false;
}

void sys_wait_for_event(Event* res) {
	if ((usize) res >= HHDM_OFFSET) {
		sched_kill_cur();
	}

	Task* self = arch_get_cur_task();
	if (event_queue_get(&self->event_queue, res)) {
		return;
	}
	else {
		sched_block(TASK_STATUS_WAITING);
		event_queue_get(&self->event_queue, res);
	}
}

bool sys_poll_event(Event* res) {
	if ((usize) res >= HHDM_OFFSET) {
		sched_kill_cur();
	}
	Task* self = arch_get_cur_task();
	return event_queue_get(&self->event_queue, res);
}

int sys_wait_thread(HandleId handle) {
	if (!handle) {
		return E_ARG;
	}
	Task* self = arch_get_cur_task();
	mutex_lock(&self->lock);
	Handle* tmp_handle = handle_list_get(&self->thread_handles, handle);
	if (!tmp_handle) {
		mutex_unlock(&self->lock);
		return E_ARG;
	}
	ThreadHandle* t_handle = container_of(tmp_handle, ThreadHandle, common);

	Task* thread = t_handle->task;

	if (thread->detached) {
		mutex_unlock(&self->lock);
		return E_DETACHED;
	}

	// unlocks the lock and yields
	sched_sigwait(thread);

	int status = t_handle->status;
	handle_list_remove(&self->thread_handles, tmp_handle);
	kfree(t_handle, sizeof(ThreadHandle));
	return status;
}

void sys_sleep(usize ms) {
	sched_sleep(ms * US_IN_MS);
}

void sys_dprint(const char* msg, size_t len) {
	if ((usize) msg >= HHDM_OFFSET) {
		sched_kill_cur();
	}
	kputs(msg, len);
}

void sys_exit(int status) {
	Task* task = arch_get_cur_task();
	kprintf("task '%s' exited with status %d\n", task->name, status);
	Ipl old = arch_ipl_set(IPL_CRITICAL);
	if (!task->detached) {
		if (task->parent) {
			mutex_lock(&task->parent->lock);
			Handle* handle = handle_list_get(&task->parent->thread_handles, task->id);
			assert(handle);
			ThreadHandle* t_handle = container_of(handle, ThreadHandle, common);
			t_handle->exited = true;
			t_handle->status = status;
			arch_ipl_set(old);
			mutex_unlock(&task->parent->lock);
		}
	}
	else {
		if (task->parent) {
			mutex_lock(&task->parent->lock);
			Handle* handle = handle_list_get(&task->parent->thread_handles, task->id);
			assert(handle);
			handle_list_remove(&task->parent->thread_handles, handle);
			kfree(container_of(handle, ThreadHandle, common), sizeof(ThreadHandle));
			arch_ipl_set(old);
			mutex_unlock(&task->parent->lock);
		}
	}

	sched_exit(status, TASK_STATUS_EXITED);
}

HandleId sys_create_thread(void (*fn)(), void* arg, bool detach) {
	Task* self = arch_get_cur_task();
	ThreadHandle* handle = kmalloc(sizeof(ThreadHandle));
	if (!handle) {
		return 0;
	}

	Task* task = arch_create_user_task_with_map("user thread", fn, arg, self, self->map, self->user_vmem, detach);
	if (!task) {
		kfree(handle, sizeof(ThreadHandle));
		return 0;
	}
	task->child_next = self->children;
	self->children = task;
	Ipl old = arch_ipl_set(IPL_CRITICAL);

	HandleId id = ++self->thread_handles.counter;
	handle->common.id = id;
	handle->task = task;
	handle->exited = false;
	handle->status = 0;
	mutex_lock(&self->lock);
	handle_list_add(&self->thread_handles, &handle->common);
	mutex_unlock(&self->lock);

	task->id = id;
	sched_queue_task(task);
	arch_ipl_set(old);
	return id;
}