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

void sys_exit(int status);
int sys_create_process(__user const char* path, size_t path_len, __user Handle* ret);
int sys_kill_process(Process* process);
Handle sys_create_thread(void (*fn)(void*), void* arg);
int sys_kill_thread(Handle handle);
int sys_dprint(__user const char* msg, size_t len);
void sys_sleep(usize ms);
int sys_wait_thread(Handle handle);
int sys_wait_process(Handle handle);
int sys_wait_for_event(__user Event* res);
int sys_poll_event(__user Event* res);
int sys_shutdown(ShutdownType type);
bool sys_request_cap(u32 cap);
void* sys_mmap(size_t size, int protection);
int sys_munmap(__user void* ptr, size_t size);
int sys_close(Handle handle);

__attribute__((used)) void* syscall_handlers[] = {
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
	[SYS_READ] = sys_read,
	[SYS_STAT] = sys_stat,
	[SYS_OPENDIR] = sys_opendir,
	[SYS_READDIR] = sys_readdir,
	[SYS_CLOSEDIR] = sys_closedir
};

__attribute__((used)) const usize syscall_handler_count = sizeof(syscall_handlers) / sizeof(*syscall_handlers);

static const char* CAP_STRS[CAP_MAX] = {
	[CAP_DIRECT_FB_ACCESS] = "DIRECT_FB_ACCESS",
	[CAP_MANAGE_POWER] = "MANAGE_POWER"
};

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

int sys_wait_for_event(__user Event* res) {
	Task* self = arch_get_cur_task();

	Event kernel_res;

	if (!event_queue_get(&self->event_queue, &kernel_res)) {
		sched_block(TASK_STATUS_WAITING);
		event_queue_get(&self->event_queue, &kernel_res);
	}

	if (!mem_copy_to_user(res, &kernel_res, sizeof(Event))) {
		return ERR_FAULT;
	}

	return 0;
}

int sys_poll_event(__user Event* res) {
	Task* self = arch_get_cur_task();
	Event kernel_res;
	bool result = event_queue_get(&self->event_queue, &kernel_res);
	if (!mem_copy_to_user(res, &kernel_res, sizeof(Event))) {
		return ERR_FAULT;
	}
	return !result;
}

int sys_kill_thread(Handle handle) {
	if (handle == INVALID_HANDLE) {
		return ERR_INVALID_ARG;
	}
	Task* self = arch_get_cur_task();

	ThreadHandle* t_handle = (ThreadHandle*) handle_tab_open(&self->process->handle_table, handle);
	if (t_handle == NULL) {
		return ERR_INVALID_ARG;
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

	return status;
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

int sys_dprint(__user const char* msg, size_t len) {
	char* buffer = kmalloc(len);
	if (!buffer) {
		return ERR_NO_MEM;
	}
	if (!mem_copy_to_kernel(buffer, msg, len)) {
		kfree(buffer, len);
		return ERR_FAULT;
	}

	kputs(buffer, len);
	kfree(buffer, len);
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
	Process* process = self->process;
	mutex_lock(&process->threads_lock);
	if (atomic_load_explicit(&process->killed, memory_order_acquire)) {
		mutex_unlock(&process->threads_lock);
		return INVALID_HANDLE;
	}

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
	mutex_unlock(&process->threads_lock);
	arch_ipl_set(old);
	return id;
}

int sys_create_process(__user const char* path, size_t path_len, __user Handle* ret) {
	char* buf = kmalloc(path_len + 1);
	if (!buf) {
		return ERR_NO_MEM;
	}
	if (!mem_copy_to_kernel(buf, path, path_len)) {
		kfree(buf, path_len + 1);
		return ERR_FAULT;
	}
	buf[path_len] = 0;

	VNode* node;
	int status = kernel_fs_open(buf, &node);
	if (status != 0) {
		kfree(buf, path_len + 1);
		return status;
	}

	Process* process = process_new_user();
	if (!process) {
		kfree(buf, path_len + 1);
		node->ops.release(node);
		return ERR_NO_MEM;
	}

	const char* process_name = buf + path_len - 1;
	for (; process_name > buf && *process_name != '/'; --process_name);

	Task* thread = arch_create_user_task(process, process_name, NULL, NULL);
	kfree(buf, path_len + 1);
	if (!thread) {
		node->ops.release(node);
		process_destroy(process);
		return ERR_NO_MEM;
	}

	LoadedElf res;
	status = elf_load_from_file(thread, node, &res);
	node->ops.release(node);
	if (status != 0) {
		arch_destroy_task(thread);
		return status;
	}

	void (*user_fn)(void*) = (void (*)(void*)) res.entry;
	arch_set_user_task_fn(thread, user_fn);

	ThreadHandle* h = kmalloc(sizeof(ThreadHandle));
	if (!h) {
		arch_destroy_task(thread);
		return ERR_NO_MEM;
	}
	h->task = thread;
	h->exited = false;
	memset(&h->lock, 0, sizeof(Mutex));
	Handle handle = handle_tab_insert(&process->handle_table, h, HANDLE_TYPE_THREAD);

	if (!mem_copy_to_user(ret, &handle, sizeof(handle))) {
		arch_destroy_task(thread);
		handle_tab_close(&process->handle_table, handle);
		kfree(h, sizeof(ThreadHandle));
		return ERR_FAULT;
	}

	process_add_thread(process, thread);

	Ipl old = arch_ipl_set(IPL_CRITICAL);
	sched_queue_task(thread);
	arch_ipl_set(old);

	return 0;
}

int sys_wait_process(Handle handle) {
	// todo implement
	return ERR_OPERATION_NOT_SUPPORTED;
}

int sys_kill_process(Process* process) {
	mutex_lock(&process->threads_lock);
	atomic_store_explicit(&process->killed, true, memory_order_release);

	for (Task* task = process->threads; task; task = task->thread_next) {
		sched_kill_task(task);
	}

	mutex_unlock(&process->threads_lock);
	return 0;
}

void* sys_mmap(size_t size, int protection) {
	if (!size || (size & (PAGE_SIZE - 1))) {
		return NULL;
	}

	MappingFlags flags = 0;
	if (protection & PROT_READ) {
		flags |= MAPPING_FLAG_R;
	}
	if (protection & PROT_WRITE) {
		flags |= MAPPING_FLAG_W;
	}
	if (protection & PROT_EXEC) {
		flags |= MAPPING_FLAG_X;
	}
	Task* self = arch_get_cur_task();
	void* res = vm_user_alloc_on_demand(self->process, NULL, size / PAGE_SIZE, flags, NULL);
	if (!res) {
		return NULL;
	}
	return res;
}

int sys_munmap(__user void* ptr, size_t size) {
	if (!size || (size & (PAGE_SIZE - 1))) {
		return ERR_INVALID_ARG;
	}

	Task* self = arch_get_cur_task();
	Mapping* mapping = process_get_mapping_for_range(self->process, (const void*) ptr, 0);
	if (!mapping || mapping->size != size) {
		return ERR_INVALID_ARG;
	}

	if (!vm_user_dealloc_on_demand(self->process, (void*) ptr, size / PAGE_SIZE, NULL)) {
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
	if (!entry || entry->type == HANDLE_TYPE_KERNEL_GENERIC) {
		return ERR_INVALID_ARG;
	}
	HandleType type = entry->type;
	void* data = entry->data;

	if (handle_tab_close(&arch_get_cur_task()->process->handle_table, handle)) {
		switch (type) {
			case HANDLE_TYPE_THREAD:
				kfree(data, sizeof(ThreadHandle));
				break;
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
		}
	}
	return 0;
}
