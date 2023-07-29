#pragma once
#include "sched/task.h"

#define KERNEL_STACK_SIZE 0x2000
#define USER_STACK_SIZE 0x2000

typedef struct X86Task {
	struct X86Task* self;
	usize rsp;
	usize kernel_rsp;
	usize syscall_save_rsp;
	usize stack_base;
	usize kernel_stack_base;
	struct Page* map_pages;
	usize ex_rsp;
	usize ex_rip;
	Task common;
	bool user;
} X86Task;

#include "usermode.inc"
static_assert(offsetof(X86Task, common) + offsetof(Task, inside_syscall) == X86TASK_INSIDE_SYSCALL_OFFSET);
static_assert(offsetof(Task, inside_syscall) == 308);
static_assert(offsetof(X86Task, common) == 72);

void x86_task_add_map_page(X86Task* task, struct Page* page);
void x86_task_remove_map_page(X86Task* task, struct Page* page);