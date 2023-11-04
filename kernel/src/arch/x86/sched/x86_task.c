#include "x86_task.h"
#include "arch/map.h"
#include "arch/x86/mem/map.h"
#include "mem/allocator.h"
#include "mem/page.h"
#include "mem/pmalloc.h"
#include "mem/utils.h"
#include "mem/vm.h"
#include "sched/sched.h"
#include "sched/sched_internals.h"
#include "string.h"
#include "sched/process.h"
#include "sys/dev.h"
#include "fs/vfs.h"
#include "sys/fs.h"
#include "arch/x86/cpu.h"

typedef struct {
	u64 r15, r14, r13, r12, rbp, rbx;
	void (*after_switch)(X86Task* old_task);
	void (*fn)(void*);
	void* arg;
	u64 null_rbp;
	u64 null_rip;
} InitStackFrame;

typedef struct {
	u64 r15, r14, r13, r12, rbp, rbx;
	void (*after_switch)(X86Task* old_task);
	void (*usermode_ret)();
	void (*fn)(void*);
	void* arg;
	void* rsp;
	u64 null_rbp;
	u64 null_rip;
} UserInitStackFrame;

static void x86_switch_from_init(X86Task* old_task) {
	sched_switch_from_init(&old_task->common);
}

extern void x86_usermode_ret();

extern void* x86_create_user_map();

usize USER_STACK_SIZE = 1024 * 1024 * 4;

Task* arch_create_sysv_user_task(Process* process, const char* name, const SysvTaskInfo* info) {
	usize total_str_mem = 0;
	for (usize i = 0; i < info->args_len; ++i) {
		total_str_mem += info->args[i].len + 1;
	}
	for (usize i = 0; i < info->env_len; ++i) {
		total_str_mem += info->env[i].len + 1;
	}
	if (total_str_mem > USER_STACK_SIZE / 4) {
		return NULL;
	}

	X86PageMap* m = (X86PageMap*) process->map;
	m->ref_count += 1;

	X86Task* task = kmalloc(sizeof(X86Task));
	if (!task) {
		kprintf("[kernel][x86]: failed to allocate user task (out of memory)\n");
		return NULL;
	}
	memset(task, 0, sizeof(X86Task));
	task->self = task;
	task->common.map = m;
	if (!event_queue_init(&task->common.event_queue)) {
		kprintf("[kernel][x86]: failed to allocate event queue (out of memory)\n");
		kfree(task, sizeof(X86Task));
		return NULL;
	}

	u8* kernel_stack = (u8*) kmalloc(KERNEL_STACK_SIZE);
	if (!kernel_stack) {
		kprintf("[kernel][x86]: failed to allocate kernel stack for user task (out of memory)\n");
		kfree(task, sizeof(X86Task));
		return NULL;
	}
	memset(kernel_stack, 0, KERNEL_STACK_SIZE);
	task->kernel_rsp = (usize) kernel_stack + KERNEL_STACK_SIZE;
	task->kernel_stack_base = (usize) kernel_stack;

	u8* stack;
	u8* user_stack = (u8*) vm_user_alloc_backed(process, NULL, USER_STACK_SIZE / PAGE_SIZE, PF_READ | PF_WRITE | PF_USER, true, (void**) &stack);
	if (!user_stack) {
		kprintf("[kernel][x86]: failed to allocate user stack (out of memory)\n");
		kfree(kernel_stack, KERNEL_STACK_SIZE);
		kfree(task, sizeof(X86Task));
		return NULL;
	}
	u8* stack_base = stack;
	memset(stack, 0, USER_STACK_SIZE);
	task->stack_base = (usize) user_stack;
	stack += USER_STACK_SIZE;
	user_stack += USER_STACK_SIZE;

	// begin sysv specific

	stack -= total_str_mem;
	user_stack -= total_str_mem;

	u8* str_ptr = stack;
	u8* user_str_ptr = user_stack;
	u64* ptr = (u64*) stack;

	// null aux
	*--ptr = 0;
	*--ptr = 0;
	stack -= 16;
	user_stack -= 16;

	for (usize i = info->aux_len; i > 0; --i) {
		*--ptr = info->aux[i - 1].value;
		*--ptr = info->aux[i - 1].type;
		stack -= 16;
		user_stack -= 16;
	}

	// aux end
	*--ptr = 0;
	stack -= 8;
	user_stack -= 8;

	for (usize i = info->env_len; i > 0; --i) {
		*--ptr = (u64) user_str_ptr;
		stack -= 8;
		user_stack -= 8;

		memcpy(str_ptr, info->env[i - 1].data, info->env[i - 1].len);
		str_ptr[info->env[i - 1].len] = 0;
		str_ptr += info->env[i - 1].len + 1;
		user_str_ptr += info->env[i - 1].len + 1;
	}

	// env end
	*--ptr = 0;
	stack -= 8;
	user_stack -= 8;

	for (usize i = info->args_len; i > 0; --i) {
		*--ptr = (u64) user_str_ptr;
		stack -= 8;
		user_stack -= 8;

		memcpy(str_ptr, info->args[i - 1].data, info->args[i - 1].len);
		str_ptr[info->args[i - 1].len] = 0;
		str_ptr += info->args[i - 1].len + 1;
		user_str_ptr += info->args[i - 1].len + 1;
	}

	// argc
	*--ptr = info->args_len;
	stack -= 8;
	user_stack -= 8;

	// end sysv specific

	UserInitStackFrame* frame = (UserInitStackFrame*) (task->kernel_rsp - sizeof(UserInitStackFrame));
	frame->after_switch = x86_switch_from_init;
	frame->usermode_ret = x86_usermode_ret;
	frame->fn = (void (*)(void*)) info->fn;
	frame->arg = NULL;
	frame->rsp = user_stack;
	frame->null_rbp = 0;
	frame->null_rip = 0;

	vm_user_dealloc_kernel(stack_base, USER_STACK_SIZE / PAGE_SIZE);

	strncpy(task->common.name, name, sizeof(task->common.name));
	task->common.status = TASK_STATUS_READY;

	task->rsp = (usize) frame;
	task->common.level = SCHED_MAX_LEVEL - 1;
	task->user = true;
	task->common.priority = 0;
	task->common.process = process;
	usize simd_size = CPU_FEATURES.xsave ? CPU_FEATURES.xsave_area_size : sizeof(FxState);
	task->state.simd = (u8*) vm_kernel_alloc_backed(ALIGNUP(simd_size, PAGE_SIZE) / PAGE_SIZE, PF_READ | PF_WRITE);
	if (!task->state.simd) {
		kprintf("[kernel][x86]: failed to allocate simd state (out of memory)\n");
		kfree(kernel_stack, KERNEL_STACK_SIZE);
		vm_user_dealloc_backed(process, (void*) task->stack_base, USER_STACK_SIZE / PAGE_SIZE, true, NULL);
		kfree(task, sizeof(X86Task));
		return NULL;
	}
	FxState* fx_state = (FxState*) task->state.simd;
	// IM | DM | ZM | OM | UM | PM | PC
	fx_state->fcw = 1 << 0 | 1 << 1 | 1 << 2 | 1 << 3 | 1 << 4 | 1 << 5 | 0b11 << 8;
	fx_state->mxcsr = 0b1111110000000;

	return &task->common;
}

Task* arch_create_user_task(Process* process, const char* name, void (*fn)(void*), void* arg) {
	X86PageMap* m = (X86PageMap*) process->map;
	m->ref_count += 1;

	X86Task* task = kmalloc(sizeof(X86Task));
	if (!task) {
		kprintf("[kernel][x86]: failed to allocate user task (out of memory)\n");
		return NULL;
	}
	memset(task, 0, sizeof(X86Task));
	task->self = task;
	task->common.map = m;
	if (!event_queue_init(&task->common.event_queue)) {
		kprintf("[kernel][x86]: failed to allocate event queue (out of memory)\n");
		kfree(task, sizeof(X86Task));
		return NULL;
	}

	u8* kernel_stack = (u8*) kmalloc(KERNEL_STACK_SIZE);
	if (!kernel_stack) {
		kprintf("[kernel][x86]: failed to allocate kernel stack for user task (out of memory)\n");
		kfree(task, sizeof(X86Task));
		return NULL;
	}
	memset(kernel_stack, 0, KERNEL_STACK_SIZE);
	task->kernel_rsp = (usize) kernel_stack + KERNEL_STACK_SIZE;
	task->kernel_stack_base = (usize) kernel_stack;

	u8* stack;
	u8* user_stack = (u8*) vm_user_alloc_backed(process, NULL, USER_STACK_SIZE / PAGE_SIZE, PF_READ | PF_WRITE | PF_USER, true, (void**) &stack);
	if (!user_stack) {
		kprintf("[kernel][x86]: failed to allocate user stack (out of memory)\n");
		kfree(kernel_stack, KERNEL_STACK_SIZE);
		kfree(task, sizeof(X86Task));
		return NULL;
	}
	u8* stack_base = stack;
	memset(stack, 0, USER_STACK_SIZE);
	task->stack_base = (usize) user_stack;
	stack += USER_STACK_SIZE;
	user_stack += USER_STACK_SIZE;

	UserInitStackFrame* frame = (UserInitStackFrame*) (task->kernel_rsp - sizeof(UserInitStackFrame));
	frame->after_switch = x86_switch_from_init;
	frame->usermode_ret = x86_usermode_ret;
	frame->fn = fn;
	frame->arg = arg;
	frame->rsp = user_stack;
	frame->null_rbp = 0;
	frame->null_rip = 0;

	vm_user_dealloc_kernel(stack_base, USER_STACK_SIZE / PAGE_SIZE);

	strncpy(task->common.name, name, sizeof(task->common.name));
	task->common.status = TASK_STATUS_READY;

	task->rsp = (usize) frame;
	task->common.level = SCHED_MAX_LEVEL - 1;
	task->user = true;
	task->common.priority = 0;
	task->common.process = process;
	usize simd_size = CPU_FEATURES.xsave ? CPU_FEATURES.xsave_area_size : sizeof(FxState);
	task->state.simd = (u8*) vm_kernel_alloc_backed(ALIGNUP(simd_size, PAGE_SIZE) / PAGE_SIZE, PF_READ | PF_WRITE);
	if (!task->state.simd) {
		kprintf("[kernel][x86]: failed to allocate simd state (out of memory)\n");
		kfree(kernel_stack, KERNEL_STACK_SIZE);
		vm_user_dealloc_backed(process, (void*) task->stack_base, USER_STACK_SIZE / PAGE_SIZE, true, NULL);
		kfree(task, sizeof(X86Task));
		return NULL;
	}
	FxState* fx_state = (FxState*) task->state.simd;
	// IM | DM | ZM | OM | UM | PM | PC
	fx_state->fcw = 1 << 0 | 1 << 1 | 1 << 2 | 1 << 3 | 1 << 4 | 1 << 5 | 0b11 << 8;
	fx_state->mxcsr = 0b1111110000000;

	return &task->common;
}

void arch_set_user_task_fn(Task* task, void (*fn)(void*)) {
	X86Task* x86_task = container_of(task, X86Task, common);

	void* page = to_virt(arch_virt_to_phys(task->map, ALIGNDOWN(x86_task->rsp, PAGE_SIZE)) + (x86_task->rsp & (PAGE_SIZE - 1)));
	UserInitStackFrame* frame = (UserInitStackFrame*) page;
	frame->fn = fn;
}

Task* arch_create_kernel_task(const char* name, void (*fn)(void*), void* arg) {
	X86Task* task = kmalloc(sizeof(X86Task));
	assert(task);
	memset(task, 0, sizeof(X86Task));
	task->self = task;
	if (!event_queue_init(&task->common.event_queue)) {
		kprintf("[kernel][x86]: failed to allocate event queue (out of memory)\n");
		kfree(task, sizeof(X86Task));
		return NULL;
	}

	u8* stack = (u8*) kmalloc(KERNEL_STACK_SIZE);
	assert(stack);
	memset(stack, 0, KERNEL_STACK_SIZE);
	task->stack_base = (usize) stack;
	stack += KERNEL_STACK_SIZE - sizeof(InitStackFrame);

	InitStackFrame* frame = (InitStackFrame*) stack;
	frame->after_switch = x86_switch_from_init;
	frame->fn = fn;
	frame->arg = arg;
	frame->null_rbp = 0;
	frame->null_rip = 0;

	strncpy(task->common.name, name, sizeof(task->common.name));
	task->common.status = TASK_STATUS_READY;

	task->rsp = (usize) stack;
	task->common.map = KERNEL_MAP;
	task->common.level = SCHED_MAX_LEVEL - 1;
	task->user = false;
	task->common.priority = 0;

	return &task->common;
}

void arch_destroy_task(Task* task) {
	X86Task* x86_task = container_of(task, X86Task, common);

	if (x86_task->user) {
		usize simd_size = CPU_FEATURES.xsave ? CPU_FEATURES.xsave_area_size : sizeof(FxState);
		assert(x86_task->state.simd);
		vm_kernel_dealloc_backed(x86_task->state.simd, ALIGNUP(simd_size, PAGE_SIZE) / PAGE_SIZE);
		x86_task->state.simd = NULL;

		vm_user_dealloc_backed(task->process, (void*) x86_task->stack_base, USER_STACK_SIZE / PAGE_SIZE, true, NULL);

		kfree((void*) (x86_task->kernel_stack_base), KERNEL_STACK_SIZE);

		// todo refcount for shared memory
		Process* process = task->process;

		if (process->thread_count == 0) {
			process_destroy(process);
		}

		arch_destroy_map(x86_task->common.map);
	}
	else {
		kfree((void*) x86_task->stack_base, KERNEL_STACK_SIZE);
	}

	kfree(x86_task, sizeof(X86Task));
}
