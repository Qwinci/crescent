#pragma once
#include "sched/task.h"

typedef struct X86Task {
	struct X86Task* self;
	usize rsp;
	usize kernel_rsp;
	usize syscall_save_rsp;
	usize stack_base;
	struct Page* map_pages;
	Task common;
	bool user;
} X86Task;

void x86_task_add_map_page(X86Task* task, struct Page* page);
void x86_task_remove_map_page(X86Task* task, struct Page* page);