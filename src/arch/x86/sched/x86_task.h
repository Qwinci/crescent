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

#define X86TASK_COMMON_OFF 72
#define TASK_HANDLER_IP_OFF 296
#define TASK_HANDLER_SP_OFF 304

#include "usermode.inc"
static_assert(offsetof(X86Task, common) == X86TASK_COMMON_OFF);
static_assert(offsetof(Task, handler_ip) == TASK_HANDLER_IP_OFF);
static_assert(offsetof(Task, handler_sp) == TASK_HANDLER_SP_OFF);
static_assert(X86TASK_COMMON_OFF + TASK_HANDLER_IP_OFF == X86TASK_HANDLER_IP_OFF);
static_assert(X86TASK_COMMON_OFF + TASK_HANDLER_SP_OFF == X86TASK_HANDLER_SP_OFF);

void x86_task_add_map_page(X86Task* task, struct Page* page);
void x86_task_remove_map_page(X86Task* task, struct Page* page);