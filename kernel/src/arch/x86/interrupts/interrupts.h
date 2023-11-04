#pragma once
#include "types.h"

typedef struct {
	u64 r15, r14, r13, r12, r11, r10, r9, r8, rbp, rsi, rdi, rdx, rcx, rbx, rax;
	u64 error;
	u64 ip;
	u64 cs;
	u64 flags;
	u64 sp;
	u64 ss;
} InterruptCtx;

const char* resolve_ip(usize ip);
void backtrace_display(bool lock);