#pragma once
#include "sched/task.h"

typedef struct X86Task {
	struct X86Task* self;
	usize rsp;
	usize kernel_rsp;
	usize syscall_save_rsp;
	usize stack_base;
	Task common;
	bool user;
} X86Task;