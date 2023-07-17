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
	u64 null_rbp;
	u64 null_rip;
} UserInitStackFrame;

static void x86_switch_from_init(X86Task* old_task) {
	sched_switch_from_init(&old_task->common);
}

extern void x86_usermode_ret();

extern void* x86_create_user_map();

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
	u8* user_stack = (u8*) vm_user_alloc_backed(process, USER_STACK_SIZE / PAGE_SIZE, PF_READ | PF_WRITE | PF_USER, (void**) &stack);
	if (!user_stack) {
		kprintf("[kernel][x86]: failed to allocate user stack (out of memory)\n");
		kfree(kernel_stack, KERNEL_STACK_SIZE);
		kfree(task, sizeof(X86Task));
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
	task->common.process = process;

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
		vm_user_dealloc_backed(task->process, (void*) x86_task->stack_base, USER_STACK_SIZE / PAGE_SIZE, NULL);

		kfree((void*) (x86_task->kernel_rsp - KERNEL_STACK_SIZE), KERNEL_STACK_SIZE);

		// todo refcount for shared memory
		Process* process = task->process;

		if (process->thread_count == 0) {
			for (MemMapping* mapping = process->mappings; mapping; mapping = mapping->next) {
				for (usize i = mapping->base; i < mapping->base + mapping->size; i += PAGE_SIZE) {
					usize phys = arch_virt_to_phys(task->map, i);
					Page* page = page_from_addr(phys);
					page->refs -= 1;
					if (page->refs == 0) {
						pfree(page, 1);
					}
				}
			}

			vm_user_free(task->process);
			arch_destroy_map(x86_task->common.map);
		}
	}
	else {
		kfree((void*) x86_task->stack_base, KERNEL_STACK_SIZE);
	}

	kfree(x86_task, sizeof(X86Task));
}

void x86_task_add_map_page(X86Task* task, struct Page* page) {
	page->prev = NULL;
	page->next = task->map_pages;
	if (page->next) {
		page->next->prev = page;
	}
	task->map_pages = page;
}

void x86_task_remove_map_page(X86Task* task, struct Page* page) {
	if (page->prev) {
		page->prev->next = page->next;
	}
	else {
		task->map_pages = page->next;
	}
	if (page->next) {
		page->next->prev = page->prev;
	}
}