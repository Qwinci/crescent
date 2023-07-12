#include "arch/cpu.h"
#include "arch/interrupts.h"
#include "arch/misc.h"
#include "mem/utils.h"
#include "sched/sched.h"
#include "stdio.h"
#include "utils/handle.h"
#include "mem/allocator.h"
#include "string.h"
#include "mem/vm.h"
#include "mem/page.h"

void sys_exit(int status);
Handle sys_create_thread(void (*fn)(void*), void* arg);
void sys_dprint(const char* msg, size_t len);
void sys_sleep(usize ms);
int sys_wait_thread(Handle handle);
void sys_wait_for_event(Event* res);
bool sys_poll_event(Event* res);
bool sys_shutdown(ShutdownType type);
bool sys_request_cap(u32 cap);
void* sys_mmap(size_t size, int protection);
int sys_munmap(void* ptr, size_t size);
int sys_close(Handle handle);

__attribute__((used)) void* syscall_handlers[] = {
	sys_exit,
	sys_create_thread,
	sys_dprint,
	sys_sleep,
	sys_wait_thread,
	sys_wait_for_event,
	sys_poll_event,
	sys_shutdown,
	sys_request_cap,
	sys_mmap,
	sys_munmap,
	sys_close
};

__attribute__((used)) usize syscall_handler_count = sizeof(syscall_handlers) / sizeof(*syscall_handlers);

#define E_DETACHED (-1)
#define E_ARG (-2)

static const char* CAP_STRS[CAP_MAX] = {
	[CAP_DIRECT_FB_ACCESS] = "DIRECT_FB_ACCESS"
};

bool sys_request_cap(u32 cap) {
	kprintf("task '%s' is requesting cap '%s', allow?\n", arch_get_cur_task()->name, CAP_STRS[cap]);
	Task* self = arch_get_cur_task();

	spinlock_lock(&ACTIVE_INPUT_TASK_LOCK);
	Task* old = ACTIVE_INPUT_TASK;
	ACTIVE_INPUT_TASK = self;
	spinlock_unlock(&ACTIVE_INPUT_TASK_LOCK);

	Event res;
	if (!event_queue_get(&self->event_queue, &res)) {
		sched_block(TASK_STATUS_WAITING);
		event_queue_get(&self->event_queue, &res);
	}
	bool allow = false;
	if (res.type == EVENT_KEY && res.key.key == SCAN_Y && res.key.pressed) {
		allow = true;
	}

	spinlock_lock(&ACTIVE_INPUT_TASK_LOCK);
	ACTIVE_INPUT_TASK = old;
	spinlock_unlock(&ACTIVE_INPUT_TASK_LOCK);
	return allow;
}

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

int sys_wait_thread(Handle handle) {
	if (handle == INVALID_HANDLE) {
		return E_ARG;
	}
	Task* self = arch_get_cur_task();

	ThreadHandle* t_handle = (ThreadHandle*) handle_tab_open(&self->process->handle_table, handle);
	if (t_handle == NULL) {
		return E_ARG;
	}
	mutex_lock(&t_handle->lock);
	if (t_handle->exited) {
		mutex_unlock(&t_handle->lock);
	}
	else {
		Task* thread = t_handle->task;
		handle_tab_close(&self->process->handle_table, handle);
		mutex_lock(&thread->signal_waiters_lock);
		mutex_unlock(&t_handle->lock);
		sched_sigwait(thread);
		t_handle = (ThreadHandle*) handle_tab_open(&self->process->handle_table, handle);
	}

	int status = t_handle->status;
	handle_tab_close(&self->process->handle_table, handle);
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
	arch_ipl_set(IPL_CRITICAL);
	sched_exit(status, TASK_STATUS_EXITED);
}

Handle sys_create_thread(void (*fn)(void*), void* arg) {
	Task* self = arch_get_cur_task();
	ThreadHandle* handle = kmalloc(sizeof(ThreadHandle));
	if (!handle) {
		return INVALID_HANDLE;
	}

	Task* task = arch_create_user_task(self->process, "user thread", fn, arg);
	if (!task) {
		kfree(handle, sizeof(ThreadHandle));
		return INVALID_HANDLE;
	}

	Ipl old = arch_ipl_set(IPL_CRITICAL);
	handle->task = task;
	handle->exited = false;
	memset(&handle->lock, 0, sizeof(Mutex));
	Handle id = handle_tab_insert(&self->process->handle_table, handle, HANDLE_TYPE_THREAD);
	task->tid = id;

	sched_queue_task(task);
	process_add_thread(self->process, task);
	arch_ipl_set(old);
	return id;
}

void* sys_mmap(size_t size, int protection) {
	if (!size || (size & (PAGE_SIZE - 1))) {
		return NULL;
	}

	PageFlags flags = PF_USER;
	if (protection & PROT_READ) {
		flags |= PF_READ;
	}
	if (protection & PROT_WRITE) {
		flags |= PF_READ | PF_WRITE;
	}
	if (protection & PROT_EXEC) {
		flags |= PF_READ | PF_EXEC;
	}
	void* res = vm_user_alloc_backed(arch_get_cur_task()->process, size / PAGE_SIZE, flags, NULL);
	if (res) {
		memset(res, 0, size);
	}
	arch_invalidate_mapping(arch_get_cur_task()->process);
	return res;
}

int sys_munmap(void* ptr, size_t size) {
	if (!ptr || (usize) ptr >= HHDM_OFFSET || !size || (size & (PAGE_SIZE - 1))) {
		return E_ARG;
	}

	vm_user_dealloc_backed(arch_get_cur_task()->process, ptr, size / PAGE_SIZE, NULL);
	arch_invalidate_mapping(arch_get_cur_task()->process);
	return 0;
}

int sys_close(Handle handle) {
	if (handle == INVALID_HANDLE) {
		return E_ARG;
	}
	Process* process = arch_get_cur_task()->process;
	HandleEntry* entry = handle_tab_get(&process->handle_table, handle);
	if (!entry) {
		return E_ARG;
	}
	HandleType type = entry->type;
	void* data = entry->data;

	if (handle_tab_close(&arch_get_cur_task()->process->handle_table, handle)) {
		switch (type) {
			case HANDLE_TYPE_THREAD:
				kfree(data, sizeof(ThreadHandle));
				break;
		}
	}
	return 0;
}
