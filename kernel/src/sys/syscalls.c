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
#include "dev/fb.h"
#include "crescent/sys.h"
#include "mem/user.h"
#include "fs/vfs.h"
#include "fs.h"
#include "exe/elf_loader.h"
#include "sched/signal.h"
#include "arch/executor.h"

void sys_exit(void* state);
void sys_create_process(void* state);
void sys_kill_process(void* state);
void sys_create_thread(void* state);
void sys_kill_thread(void* state);
void sys_dprint(void* state);
void sys_sleep(void* state);
void sys_wait_thread(void* state);
void sys_wait_process(void* state);
void sys_wait_for_event(void* state);
void sys_poll_event(void* state);
void sys_shutdown(void* state);
void sys_request_cap(void* state);
void sys_mmap(void* state);
void sys_munmap(void* state);
void sys_close(void* state);
void sys_raise_signal(void* state);
void sys_sigleave(void* state);
void sys_write_fs_base(void* state);
void sys_write_gs_base(void* state);

typedef void (*SyscallHandler)(void* state);

static const SyscallHandler syscall_handlers[] = {
	[SYS_CREATE_PROCESS] = sys_create_process,
	[SYS_KILL_PROCESS] = sys_kill_process,
	[SYS_WAIT_PROCESS] = sys_wait_process,
	[SYS_CREATE_THREAD] = sys_create_thread,
	[SYS_KILL_THREAD] = sys_kill_thread,
	[SYS_WAIT_THREAD] = sys_wait_thread,
	[SYS_EXIT] = sys_exit,
	[SYS_SLEEP] = sys_sleep,

	[SYS_WAIT_FOR_EVENT] = sys_wait_for_event,
	[SYS_POLL_EVENT] = sys_poll_event,

	[SYS_DPRINT] = sys_dprint,
	[SYS_SHUTDOWN] = sys_shutdown,
	[SYS_REQUEST_CAP] = sys_request_cap,

	[SYS_MMAP] = sys_mmap,
	[SYS_MUNMAP] = sys_munmap,

	[SYS_CLOSE] = sys_close,
	[SYS_DEVMSG] = sys_devmsg,
	[SYS_DEVENUM] = sys_devenum,

	[SYS_OPEN] = sys_open,
	[SYS_OPEN_EX] = sys_open_ex,
	[SYS_READ] = sys_read,
	[SYS_WRITE] = sys_write,
	[SYS_SEEK] = sys_seek,
	[SYS_STAT] = sys_stat,
	[SYS_OPENDIR] = sys_opendir,
	[SYS_READDIR] = sys_readdir,
	[SYS_CLOSEDIR] = sys_closedir,
	[SYS_ATTACH_SIGNAL] = sys_attach_signal,
	[SYS_DETACH_SIGNAL] = sys_detach_signal,
	[SYS_RAISE_SIGNAL] = sys_raise_signal,
	[SYS_SIGLEAVE] = sys_sigleave,

	[SYS_WRITE_FS_BASE] = sys_write_fs_base,
	[SYS_WRITE_GS_BASE] = sys_write_gs_base,

	[SYS_POSIX_OPEN] = sys_posix_open,
	[SYS_POSIX_READ] = sys_posix_read,
	[SYS_POSIX_SEEK] = sys_posix_seek,
	[SYS_POSIX_MMAP] = sys_posix_mmap,
	[SYS_POSIX_CLOSE] = sys_posix_close,
	[SYS_POSIX_WRITE] = sys_posix_write,
	[SYS_POSIX_FUTEX_WAIT] = sys_posix_futex_wait,
	[SYS_POSIX_FUTEX_WAKE] = sys_posix_futex_wake
};

const usize syscall_handler_count = sizeof(syscall_handlers) / sizeof(*syscall_handlers);

void syscall_dispatcher(void* state) {
	size_t num = executor_num(state);
	if (num >= syscall_handler_count) {
		*executor_ret0(state) = ERR_INVALID_ARG;
		return;
	}
	syscall_handlers[num](state);
}

static const char* CAP_STRS[CAP_MAX] = {
	[CAP_DIRECT_FB_ACCESS] = "DIRECT_FB_ACCESS",
	[CAP_MANAGE_POWER] = "MANAGE_POWER"
};

void sys_request_cap(void* state) {
	u32 cap = *executor_arg0(state);
	if (cap >= CAP_MAX) {
		*executor_ret0(state) = 0;
		return;
	}

	kprintf("task '%s' is requesting cap '%s', allow?\n", arch_get_cur_task()->name, CAP_STRS[cap]);
	Task* self = arch_get_cur_task();

	spinlock_lock(&ACTIVE_INPUT_TASK_LOCK);
	Task* old = ACTIVE_INPUT_TASK;
	ACTIVE_INPUT_TASK = self;
	spinlock_unlock(&ACTIVE_INPUT_TASK_LOCK);

	bool allow;
	while (true) {
		CrescentEvent res;
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
	*executor_ret0(state) = allow;
}

void sys_shutdown(void* state) {
	CrescentShutdownType type = (CrescentShutdownType) *executor_arg0(state);
	Task* task = arch_get_cur_task();
	/*if (!(task->caps & CAP_MANAGE_POWER)) {
		return ERR_NO_PERMISSIONS;
	}*/

	if (type == SHUTDOWN_TYPE_REBOOT) {
		arch_reboot();
	}
	*executor_ret0(state) = ERR_INVALID_ARG;
}

void sys_wait_for_event(void* state) {
	__user CrescentEvent* res = (__user CrescentEvent*) *executor_arg0(state);

	Task* self = arch_get_cur_task();

	CrescentEvent kernel_res;

	if (!event_queue_get(&self->event_queue, &kernel_res)) {
		sched_block(TASK_STATUS_WAITING);
		event_queue_get(&self->event_queue, &kernel_res);
	}

	if (!mem_copy_to_user(res, &kernel_res, sizeof(CrescentEvent))) {
		*executor_ret0(state) = ERR_FAULT;
		return;
	}

	*executor_ret0(state) = 0;
}

void sys_poll_event(void* state) {
	__user CrescentEvent* res = (__user CrescentEvent*) *executor_arg0(state);

	Task* self = arch_get_cur_task();
	CrescentEvent kernel_res;
	bool result = event_queue_get(&self->event_queue, &kernel_res);
	if (!mem_copy_to_user(res, &kernel_res, sizeof(CrescentEvent))) {
		*executor_ret0(state) = ERR_FAULT;
		return;
	}
	*executor_ret0(state) = !result;
}

void sys_kill_thread(void* state) {
	CrescentHandle handle = (CrescentHandle) *executor_arg0(state);

	if (handle == INVALID_HANDLE) {
		*executor_ret0(state) = ERR_INVALID_ARG;
		return;
	}
	Task* self = arch_get_cur_task();

	ThreadHandle* t_handle = (ThreadHandle*) handle_tab_open(&self->process->handle_table, handle);
	if (t_handle == NULL) {
		*executor_ret0(state) = ERR_INVALID_ARG;
		return;
	}
	mutex_lock(&t_handle->lock);
	int status;
	if (t_handle->exited) {
		status = 1;
	}
	else {
		Task* thread = t_handle->task;
		sched_kill_task(thread);
		status = 0;
	}
	mutex_unlock(&t_handle->lock);

	*executor_ret0(state) = status;
	return;
}

void sys_wait_thread(void* state) {
	CrescentHandle handle = (CrescentHandle) *executor_arg0(state);

	if (handle == INVALID_HANDLE) {
		*executor_ret0(state) = ERR_INVALID_ARG;
		return;
	}
	Task* self = arch_get_cur_task();

	ThreadHandle* t_handle = (ThreadHandle*) handle_tab_open(&self->process->handle_table, handle);
	if (t_handle == NULL) {
		*executor_ret0(state) = ERR_INVALID_ARG;
		return;
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
	*executor_ret0(state) = status;
}

void sys_sleep(void* state) {
	usize ms = (usize) *executor_arg0(state);
	sched_sleep(ms * US_IN_MS);
}

void sys_dprint(void* state) {
	__user const char* msg = (__user const char*) *executor_arg0(state);
	size_t len = (size_t) *executor_arg1(state);

	char* buffer = kmalloc(len);
	if (!buffer) {
		*executor_ret0(state) = ERR_NO_MEM;
		return;
	}
	if (!mem_copy_to_kernel(buffer, msg, len)) {
		kfree(buffer, len);
		*executor_ret0(state) = ERR_FAULT;
		return;
	}

	kputs(buffer, len);
	kfree(buffer, len);
	*executor_ret0(state) = 0;
}

void sys_exit(void* state) {
	int status = (int) *executor_arg0(state);
	Task* task = arch_get_cur_task();
	kprintf("task '%s' exited with status %d\n", task->name, status);
	arch_ipl_set(IPL_CRITICAL);
	sched_exit(status, TASK_STATUS_EXITED);
}

void sys_create_thread(void* state) {
	void (*fn)(void*) = (void (*)(void*)) *executor_arg0(state);
	void* arg = (void*) *executor_arg1(state);

	if ((usize) fn >= HHDM_OFFSET) {
		*executor_ret0(state) = INVALID_HANDLE;
		return;
	}

	Task* self = arch_get_cur_task();
	Process* process = self->process;
	mutex_lock(&process->threads_lock);
	if (atomic_load_explicit(&process->killed, memory_order_acquire)) {
		mutex_unlock(&process->threads_lock);
		*executor_ret0(state) = INVALID_HANDLE;
		return;
	}

	ThreadHandle* handle = kmalloc(sizeof(ThreadHandle));
	if (!handle) {
		mutex_unlock(&process->threads_lock);
		*executor_ret0(state) = INVALID_HANDLE;
		return;
	}
	handle->refcount = 1;

	Task* task = arch_create_user_task(self->process, "user thread", fn, arg);
	if (!task) {
		kfree(handle, sizeof(ThreadHandle));
		mutex_unlock(&process->threads_lock);
		*executor_ret0(state) = INVALID_HANDLE;
		return;
	}

	Ipl old = arch_ipl_set(IPL_CRITICAL);
	handle->task = task;
	handle->exited = false;
	memset(&handle->lock, 0, sizeof(Mutex));
	CrescentHandle id = handle_tab_insert(&self->process->handle_table, handle, HANDLE_TYPE_THREAD);
	task->tid = id;

	sched_queue_task(task);
	process_add_thread(self->process, task);
	mutex_unlock(&process->threads_lock);
	arch_ipl_set(old);
	*executor_ret0(state) = (CrescentHandle) id;
}

void sys_create_process(void* state) {
	__user const char* path = (__user const char*) *executor_arg0(state);
	size_t path_len = (size_t) *executor_arg1(state);
	__user CrescentHandle* ret = (__user CrescentHandle*) *executor_arg2(state);

	char* buf = kmalloc(path_len + 1);
	if (!buf) {
		*executor_ret0(state) = ERR_NO_MEM;
		return;
	}
	if (!mem_copy_to_kernel(buf, path, path_len)) {
		kfree(buf, path_len + 1);
		*executor_ret0(state) = ERR_FAULT;
		return;
	}
	buf[path_len] = 0;

	VNode* node;
	int status = kernel_fs_open(buf, &node);
	if (status != 0) {
		kfree(buf, path_len + 1);
		*executor_ret0(state) = status;
		return;
	}

	Process* process = process_new_user();
	if (!process) {
		kfree(buf, path_len + 1);
		node->ops.release(node);
		*executor_ret0(state) = ERR_NO_MEM;
		return;
	}

	const char* process_name = buf + path_len - 1;
	for (; process_name > buf && process_name[-1] != '/'; --process_name);

	Task* thread = arch_create_user_task(process, process_name, NULL, NULL);
	kfree(buf, path_len + 1);
	if (!thread) {
		node->ops.release(node);
		process_destroy(process);
		*executor_ret0(state) = ERR_NO_MEM;
		return;
	}

	LoadedElf res;
	// todo interp here too
	status = elf_load_from_file(process, node, &res, false, NULL);
	node->ops.release(node);
	if (status != 0) {
		thread->process->thread_count = 0;
		arch_destroy_task(thread);
		*executor_ret0(state) = status;
		return;
	}

	void (*user_fn)(void*) = (void (*)(void*)) res.entry;
	arch_set_user_task_fn(thread, user_fn);

	ProcessHandle* h = kcalloc(sizeof(ProcessHandle));
	if (!h) {
		arch_destroy_task(thread);
		*executor_ret0(state) = ERR_NO_MEM;
		return;
	}

	ThreadHandle* thread_h = kcalloc(sizeof(ThreadHandle));
	if (!thread_h) {
		arch_destroy_task(thread);
		kfree(h, sizeof(ProcessHandle));
		*executor_ret0(state) = ERR_NO_MEM;
		return;
	}
	thread_h->refcount = 1;
	thread_h->task = thread;
	thread_h->exited = false;

	h->process = process;
	h->exited = false;
	Task* task = arch_get_cur_task();
	CrescentHandle handle = handle_tab_insert(&task->process->handle_table, h, HANDLE_TYPE_PROCESS);

	if (!mem_copy_to_user(ret, &handle, sizeof(handle))) {
		handle_tab_close(&task->process->handle_table, handle);
		arch_destroy_task(thread);
		kfree(h, sizeof(ProcessHandle));
		*executor_ret0(state) = ERR_FAULT;
		return;
	}

	process_add_thread(process, thread);
	thread->tid = handle_tab_insert(&process->handle_table, thread_h, HANDLE_TYPE_THREAD);

	Ipl old = arch_ipl_set(IPL_CRITICAL);
	sched_queue_task(thread);
	arch_ipl_set(old);

	*executor_ret0(state) = 0;
}

void sys_wait_process(void* state) {
	CrescentHandle handle = (CrescentHandle) *executor_arg0(state);
	// todo implement
	*executor_ret0(state) = ERR_OPERATION_NOT_SUPPORTED;
}

void sys_kill_process(void* state) {
	CrescentHandle process_handle = (CrescentHandle) *executor_arg0(state);

	Task* this_task = arch_get_cur_task();
	HandleEntry* entry = handle_tab_get(&this_task->process->handle_table, process_handle);
	if (!entry || entry->type != HANDLE_TYPE_PROCESS) {
		*executor_ret0(state) = ERR_INVALID_ARG;
		return;
	}
	ProcessHandle* h = (ProcessHandle*) entry->data;
	if (h->exited) {
		*executor_ret0(state) = 0;
		return;
	}

	mutex_lock(&h->lock);
	Process* process = h->process;

	mutex_lock(&process->threads_lock);
	atomic_store_explicit(&process->killed, true, memory_order_release);

	for (Task* task = process->threads; task; task = task->thread_next) {
		sched_kill_task(task);
	}

	mutex_unlock(&process->threads_lock);
	mutex_unlock(&h->lock);
	*executor_ret0(state) = 0;
}

void sys_mmap(void* state) {
	size_t size = (size_t) *executor_arg0(state);
	int protection = (int) *executor_arg1(state);

	if (!size || (size & (PAGE_SIZE - 1))) {
		*(void**) executor_ret0(state) = NULL;
		return;
	}

	MappingFlags flags = 0;
	if (protection & KPROT_READ) {
		flags |= MAPPING_FLAG_R;
	}
	if (protection & KPROT_WRITE) {
		flags |= MAPPING_FLAG_W;
	}
	if (protection & KPROT_EXEC) {
		flags |= MAPPING_FLAG_X;
	}
	Task* self = arch_get_cur_task();
	void* res = vm_user_alloc_on_demand(self->process, NULL, size / PAGE_SIZE, flags, false, NULL);
	if (!res) {
		*(void**) executor_ret0(state) = NULL;
		return;
	}
	*(void**) executor_ret0(state) = res;
}

void sys_munmap(void* state) {
	__user void* ptr = (__user void*) *executor_arg0(state);
	size_t size = (size_t) *executor_arg0(state);

	if (!size || (size & (PAGE_SIZE - 1))) {
		*executor_ret0(state) = ERR_INVALID_ARG;
		return;
	}

	Task* self = arch_get_cur_task();
	Mapping* mapping = process_get_mapping_for_range(self->process, (const void*) ptr, 0);
	if (!mapping || mapping->size != size) {
		*executor_ret0(state) = ERR_INVALID_ARG;
		return;
	}

	if (!vm_user_dealloc_on_demand(self->process, (void*) ptr, size / PAGE_SIZE, false, NULL)) {
		*executor_ret0(state) = ERR_INVALID_ARG;
		return;
	}
	arch_invalidate_mapping(arch_get_cur_task()->process);
	*executor_ret0(state) = 0;
}

void sys_close(void* state) {
	CrescentHandle handle = (CrescentHandle) *executor_arg0(state);

	if (handle == INVALID_HANDLE) {
		*executor_ret0(state) = ERR_INVALID_ARG;
		return;
	}
	Process* process = arch_get_cur_task()->process;
	HandleEntry* entry = handle_tab_get(&process->handle_table, handle);
	if (!entry || entry->type == HANDLE_TYPE_KERNEL_GENERIC) {
		*executor_ret0(state) = ERR_INVALID_ARG;
		return;
	}
	HandleType type = entry->type;
	void* data = entry->data;

	if (handle_tab_close(&arch_get_cur_task()->process->handle_table, handle)) {
		switch (type) {
			case HANDLE_TYPE_THREAD:
			{
				ThreadHandle* h = (ThreadHandle*) data;
				if (--h->refcount == 0) {
					kfree(data, sizeof(ThreadHandle));
				}
				break;
			}
			case HANDLE_TYPE_DEVICE:
				((GenericDevice*) data)->refcount -= 1;
				break;
			case HANDLE_TYPE_VNODE:
				((VNode*) data)->ops.release((VNode*) data);
				break;
			case HANDLE_TYPE_KERNEL_GENERIC:
				panic("kernel generic handle shouldn't ever get closed");
			case HANDLE_TYPE_FILE:
			{
				FileData* f = (FileData*) data;
				f->node->ops.release(f->node);
				kfree(f, sizeof(FileData));
				break;
			}
			case HANDLE_TYPE_PROCESS:
				kfree(data, sizeof(ProcessHandle));
				break;
		}
	}
	*executor_ret0(state) = 0;
}

void sys_raise_signal(void* state) {
	int num = (int) *executor_arg0(state);
	*executor_ret0(state) = raise_signal(arch_get_cur_task()->process, num);
}

void sys_sigleave(void* state) {
	arch_signal_leave(state);
}

#ifdef __x86_64
#include "arch/x86/cpu.h"
#endif

void sys_write_fs_base(void* state) {
	size_t value = (size_t) *executor_arg0(state);

#ifdef __x86_64__
	x86_set_msr(MSR_FSBASE, value);
	*executor_ret0(state) = 0;
#else
	*executor_ret0(state) = ERR_OPERATION_NOT_SUPPORTED;
#endif
}

void sys_write_gs_base(void* state) {
	size_t value = (size_t) *executor_arg0(state);

#ifdef __x86_64__
	x86_set_msr(MSR_KERNELGSBASE, value);
	*executor_ret0(state) = 0;
#else
	*executor_ret0(state) = ERR_OPERATION_NOT_SUPPORTED;
#endif
}
