#pragma once
#include "sched/task.h"
#include "arch/x86/sched/executor.h"

/*
num a0  a1  a2  a3  a4  a5
rdi rsi rdx rcx r8  r9  stack

rdi rsi rdx r10 r8  r9  rax
 */

static inline size_t executor_num(void* state) {
	return (size_t) ((ExecutorGpState*) state)->rdi;
}

static inline size_t* executor_arg0(void* state) {
	return (size_t*) &((ExecutorGpState*) state)->rsi;
}

static inline size_t* executor_arg1(void* state) {
	return (size_t*) &((ExecutorGpState*) state)->rdx;
}

static inline size_t* executor_arg2(void* state) {
	return (size_t*) &((ExecutorGpState*) state)->r10;
}

static inline size_t* executor_arg3(void* state) {
	return (size_t*) &((ExecutorGpState*) state)->r8;
}

static inline size_t* executor_arg4(void* state) {
	return (size_t*) &((ExecutorGpState*) state)->r9;
}

static inline size_t* executor_arg5(void* state) {
	return (size_t*) &((ExecutorGpState*) state)->rax;
}

static inline size_t* executor_ret0(void* state) {
	return (size_t*) &((ExecutorGpState*) state)->rax;
}

void syscall_dispatcher(void* state);
