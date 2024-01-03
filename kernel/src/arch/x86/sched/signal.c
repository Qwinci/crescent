#include "signal.h"
#include "arch/executor.h"
#include "arch/misc.h"
#include "arch/x86/interrupts/interrupts.h"
#include "crescent/sys.h"
#include "sched/signal.h"
#include "stdio.h"

void process_possible_signals(InterruptCtx* ctx) {
	SignalHandler handler;
	int num;
	if (!get_next_signal(&num, &handler) || !handler.fn) {
		return;
	}
	if (ctx->cs != 0x2B) {
		panic("[kernel][x86]: signal on kernel task\n");
	}

	u64 new_sp = ctx->sp - sizeof(InterruptCtx);
	if (new_sp % 16 == 0) {
		new_sp -= 8;
	}
	if (!arch_mem_copy_to_user((__user void*) new_sp, ctx, sizeof(InterruptCtx))) {
		// todo fix this
		panic("[kernel][x86]: ran out of stack space in process_possible_signals!\n");
	}
	ctx->sp = new_sp;
	ctx->rax = 0;
	ctx->rbx = 0;
	ctx->rcx = 0;
	ctx->rdi = (u64) num;
	ctx->rsi = (u64) handler.arg;
	ctx->rdx = (u64) new_sp;
	ctx->rbp = 0;
	ctx->r8 = 0;
	ctx->r9 = 0;
	ctx->r10 = 0;
	ctx->r11 = 0;
	ctx->r12 = 0;
	ctx->r13 = 0;
	ctx->r14 = 0;
	ctx->r15 = 0;
	ctx->ip = (u64) handler.fn;
}

void arch_signal_leave(void* state) {
	__user InterruptCtx* user_ctx = (__user InterruptCtx*) *executor_arg0(state);
	InterruptCtx ctx;
	if (!arch_mem_copy_to_kernel(&ctx, user_ctx, sizeof(InterruptCtx))) {
		*executor_ret0(state) = ERR_FAULT;
		return;
	}

	ExecutorGpState* gp_state = (ExecutorGpState*) state;
	gp_state->rax = ctx.rax;
	gp_state->rbx = ctx.rbx;
	gp_state->rcx = ctx.rcx;
	gp_state->rdx = ctx.rdx;
	gp_state->rdi = ctx.rdi;
	gp_state->rsi = ctx.rsi;
	gp_state->rbp = ctx.rbp;
	gp_state->r8 = ctx.r8;
	gp_state->r9 = ctx.r9;
	gp_state->r10 = ctx.r10;
	gp_state->r11 = ctx.r11;
	gp_state->r12 = ctx.r12;
	gp_state->r13 = ctx.r13;
	gp_state->r14 = ctx.r14;
	gp_state->r15 = ctx.r15;
	gp_state->rip = ctx.ip;
	gp_state->cs = ctx.cs;
	gp_state->rflags = ctx.flags;
	gp_state->rsp = ctx.sp;
	gp_state->ss = ctx.ss;
}
