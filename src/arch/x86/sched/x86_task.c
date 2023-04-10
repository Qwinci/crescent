#include "x86_task.h"
#include "arch/map.h"
#include "mem/allocator.h"
#include "mem/page.h"
#include "mem/pmalloc.h"
#include "mem/utils.h"
#include "mem/vm.h"
#include "sched/sched.h"
#include "sched/sched_internals.h"
#include "string.h"

#define KERNEL_STACK_SIZE 0x2000
#define USER_STACK_SIZE 0x2000

typedef struct {
	u64 r15, r14, r13, r12, rbp, rbx;
	void (*after_switch)(X86Task* old_task);
	void (*fn)();
	void* arg;
	u64 null_rbp;
	u64 null_rip;
} InitStackFrame;

typedef struct {
	u64 r15, r14, r13, r12, rbp, rbx;
	void (*after_switch)(X86Task* old_task);
	void (*usermode_ret)();
	void (*fn)();
	void* arg;
	u64 null_rbp;
	u64 null_rip;
} UserInitStackFrame;

static void x86_switch_from_init(X86Task* old_task) {
	sched_switch_from_init(&old_task->common);
}

extern void x86_usermode_ret();

extern void* x86_create_user_map();

Task* arch_create_user_task(const char* name, void (*fn)(), void* arg, Task* parent) {
	X86Task* task = kmalloc(sizeof(X86Task));
	if (!task) {
		return NULL;
	}
	memset(task, 0, sizeof(X86Task));
	task->self = task;
	task->common.map = x86_create_user_map();
	vm_user_init(&task->common, 0xFF000, 0x20000);

	u8* kernel_stack = (u8*) kmalloc(KERNEL_STACK_SIZE);
	if (!kernel_stack) {
		vm_user_free(&task->common);
		arch_destroy_map(task->common.map);
		return NULL;
	}
	memset(kernel_stack, 0, KERNEL_STACK_SIZE);
	task->kernel_rsp = (usize) kernel_stack + KERNEL_STACK_SIZE;

	u8* stack;
	u8* user_stack = (u8*) vm_user_alloc_backed(&task->common, USER_STACK_SIZE / PAGE_SIZE, PF_READ | PF_WRITE | PF_USER, (void**) &stack);
	if (!user_stack) {
		kfree(kernel_stack, KERNEL_STACK_SIZE);
		vm_user_free(&task->common);
		arch_destroy_map(task->common.map);
		return NULL;
	}
	u8* stack_base = stack;
	memset(stack, 0, USER_STACK_SIZE);
	task->stack_base = (usize) user_stack;
	stack += USER_STACK_SIZE - sizeof(UserInitStackFrame);
	user_stack += USER_STACK_SIZE - sizeof(UserInitStackFrame);

	UserInitStackFrame* frame = (UserInitStackFrame*) stack;
	frame->after_switch = x86_switch_from_init;
	frame->usermode_ret = x86_usermode_ret;
	frame->fn = fn;
	frame->arg = arg;
	frame->null_rbp = 0;
	frame->null_rip = 0;

	vm_user_dealloc_kernel(stack_base, USER_STACK_SIZE / PAGE_SIZE);

	strncpy(task->common.name, name, sizeof(task->common.name));
	task->common.status = TASK_STATUS_READY;

	task->rsp = (usize) user_stack;
	task->common.level = SCHED_MAX_LEVEL - 1;
	task->user = true;
	task->common.priority = 0;
	task->common.parent = parent;

	return &task->common;
}

void arch_set_user_task_fn(Task* task, void (*fn)()) {
	X86Task* x86_task = container_of(task, X86Task, common);

	void* page = to_virt(arch_virt_to_phys(task->map, ALIGNDOWN(x86_task->rsp, PAGE_SIZE)) + (x86_task->rsp & (PAGE_SIZE - 1)));
	UserInitStackFrame* frame = (UserInitStackFrame*) page;
	frame->fn = fn;
}

Task* arch_create_kernel_task(const char* name, void (*fn)(), void* arg) {
	X86Task* task = kmalloc(sizeof(X86Task));
	memset(task, 0, sizeof(X86Task));
	task->self = task;

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
		vm_user_dealloc_backed(&x86_task->common, (void*) x86_task->stack_base, USER_STACK_SIZE / PAGE_SIZE, NULL);

		kfree((void*) (x86_task->kernel_rsp - KERNEL_STACK_SIZE), KERNEL_STACK_SIZE);

		for (Page* page = x86_task->common.allocated_pages; page;) {
			Page* next = page->next;
			pfree(page, 1);
			page = next;
		}

		vm_user_free(task);

		arch_destroy_map(x86_task->common.map);
	}
	else {
		kfree((void*) x86_task->stack_base, KERNEL_STACK_SIZE);
	}

	kfree(x86_task, sizeof(X86Task));
}