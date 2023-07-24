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
#include "crescent/fb.h"
#include "dev/fb.h"
#include "utils.h"

void sys_exit(int status);
Handle sys_create_thread(void (*fn)(void*), void* arg);
int sys_dprint(const char* msg, size_t len);
void sys_sleep(usize ms);
int sys_wait_thread(Handle handle);
int sys_wait_for_event(Event* res);
int sys_poll_event(Event* res);
int sys_shutdown(ShutdownType type);
bool sys_request_cap(u32 cap);
void* sys_mmap(size_t size, int protection);
int sys_munmap(void* ptr, size_t size);
int sys_close(Handle handle);
int sys_enumerate_framebuffers(SysFramebuffer* res, size_t* count);

__attribute__((used)) void* syscall_handlers[] = {
	sys_create_thread,
	sys_wait_thread,
	sys_exit,
	sys_sleep,

	sys_wait_for_event,
	sys_poll_event,

	sys_dprint,
	sys_shutdown,
	sys_request_cap,

	sys_mmap,
	sys_munmap,

	sys_close,

	sys_enumerate_framebuffers
};

__attribute__((used)) usize syscall_handler_count = sizeof(syscall_handlers) / sizeof(*syscall_handlers);

static const char* CAP_STRS[CAP_MAX] = {
	[CAP_DIRECT_FB_ACCESS] = "DIRECT_FB_ACCESS",
	[CAP_MANAGE_POWER] = "MANAGE_POWER"
};

#define VALIDATE_PTR(ptr) if ((usize) (ptr) >= HHDM_OFFSET) return ERR_INVALID_ARG

bool sys_request_cap(u32 cap) {
	if (cap >= CAP_MAX) {
		return false;
	}

	kprintf("task '%s' is requesting cap '%s', allow?\n", arch_get_cur_task()->name, CAP_STRS[cap]);
	Task* self = arch_get_cur_task();

	spinlock_lock(&ACTIVE_INPUT_TASK_LOCK);
	Task* old = ACTIVE_INPUT_TASK;
	ACTIVE_INPUT_TASK = self;
	spinlock_unlock(&ACTIVE_INPUT_TASK_LOCK);

	bool allow;
	while (true) {
		Event res;
		if (!event_queue_get(&self->event_queue, &res)) {
			sched_block(TASK_STATUS_WAITING);
			event_queue_get(&self->event_queue, &res);
		}
		if (res.type == EVENT_KEY && res.key.key == SCAN_Y && res.key.pressed) {
			allow = true;
			break;
		}
		else if (res.type == EVENT_KEY && res.key.pressed) {
			allow = false;
			break;
		}
	}

	if (allow) {
		self->caps |= cap;
	}

	spinlock_lock(&ACTIVE_INPUT_TASK_LOCK);
	ACTIVE_INPUT_TASK = old;
	spinlock_unlock(&ACTIVE_INPUT_TASK_LOCK);
	return allow;
}

int sys_shutdown(ShutdownType type) {
	Task* task = arch_get_cur_task();
	/*if (!(task->caps & CAP_MANAGE_POWER)) {
		return ERR_NO_PERMISSIONS;
	}*/

	if (type == SHUTDOWN_TYPE_REBOOT) {
		arch_reboot();
	}
	return ERR_INVALID_ARG;
}

int sys_wait_for_event(Event* res) {
	VALIDATE_PTR(res);

	Task* self = arch_get_cur_task();
	start_catch_faults();
	if (!event_queue_get(&self->event_queue, res)) {
		sched_block(TASK_STATUS_WAITING);
		event_queue_get(&self->event_queue, res);
	}
	end_catch_faults();
	return 0;
}

int sys_poll_event(Event* res) {
	VALIDATE_PTR(res);
	Task* self = arch_get_cur_task();
	start_catch_faults();
	bool result = event_queue_get(&self->event_queue, res);
	end_catch_faults();
	return !result;
}

int sys_wait_thread(Handle handle) {
	if (handle == INVALID_HANDLE) {
		return ERR_INVALID_ARG;
	}
	Task* self = arch_get_cur_task();

	ThreadHandle* t_handle = (ThreadHandle*) handle_tab_open(&self->process->handle_table, handle);
	if (t_handle == NULL) {
		return ERR_INVALID_ARG;
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
	assert(t_handle->exited);
	handle_tab_close(&self->process->handle_table, handle);
	return status;
}

void sys_sleep(usize ms) {
	sched_sleep(ms * US_IN_MS);
}

int sys_dprint(const char* msg, size_t len) {
	VALIDATE_PTR(msg);
	start_catch_faults();
	kputs(msg, len);
	end_catch_faults();
	return 0;
}

void sys_exit(int status) {
	Task* task = arch_get_cur_task();
	kprintf("task '%s' exited with status %d\n", task->name, status);
	arch_ipl_set(IPL_CRITICAL);
	sched_exit(status, TASK_STATUS_EXITED);
}

Handle sys_create_thread(void (*fn)(void*), void* arg) {
	if ((usize) fn >= HHDM_OFFSET) {
		return INVALID_HANDLE;
	}

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
	Task* self = arch_get_cur_task();
	void* res = vm_user_alloc_backed(self->process, size / PAGE_SIZE, flags, NULL);
	arch_invalidate_mapping(arch_get_cur_task()->process);
	return res;
}

int sys_munmap(void* ptr, size_t size) {
	VALIDATE_PTR(ptr);

	if (!ptr || !size || (size & (PAGE_SIZE - 1))) {
		return ERR_INVALID_ARG;
	}

	if (!vm_user_dealloc_backed(arch_get_cur_task()->process, ptr, size / PAGE_SIZE, NULL)) {
		return ERR_INVALID_ARG;
	}
	arch_invalidate_mapping(arch_get_cur_task()->process);
	return 0;
}

int sys_close(Handle handle) {
	if (handle == INVALID_HANDLE) {
		return ERR_INVALID_ARG;
	}
	Process* process = arch_get_cur_task()->process;
	HandleEntry* entry = handle_tab_get(&process->handle_table, handle);
	if (!entry) {
		return ERR_INVALID_ARG;
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

int sys_enumerate_framebuffers(SysFramebuffer* res, size_t* count) {
	Task* self = arch_get_cur_task();
	if (!(self->caps & CAP_DIRECT_FB_ACCESS)) {
		return ERR_NO_PERMISSIONS;
	}

	VALIDATE_PTR(res);
	VALIDATE_PTR(count);

	// todo support more than one
	if (primary_fb) {
		if (count) {
			*count = 1;
		}
		if (count && res) {
			usize size = primary_fb->height * primary_fb->pitch;
			void* mem = vm_user_alloc(self->process, ALIGNUP(size, PAGE_SIZE) / PAGE_SIZE);
			if (!mem) {
				return ERR_NO_MEM;
			}
			for (usize i = 0; i < size; i += PAGE_SIZE) {
				if (!arch_user_map_page(self->process, (usize) mem + i, to_phys(primary_fb->base) + i, PF_READ | PF_WRITE | PF_WC | PF_USER)) {
					return ERR_NO_MEM;
				}
			}

			SysFramebuffer safe_res = *primary_fb;
			safe_res.base = mem;

			start_catch_faults();
			memcpy(res, &safe_res, sizeof(SysFramebuffer));
			end_catch_faults();
		}
	}
	else {
		return 1;
	}

	return 0;
}
