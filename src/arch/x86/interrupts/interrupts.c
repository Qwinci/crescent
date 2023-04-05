#include "arch/interrupts.h"
#include "stdio.h"

static u32 used_ints[256 / 32 - 1] = {};

static HandlerData handlers[256] = {};

u32 arch_alloc_int(IntHandler handler, void* userdata) {
	for (u16 i = 0; i < 256 - 32; ++i) {
		if (!(used_ints[i / 32] & 1 << (i % 32))) {
			used_ints[i / 32] |= 1 << (i % 32);
			handlers[i + 32].handler = handler;
			handlers[i + 32].userdata = userdata;
			return i + 32;
		}
	}
	return 0;
}

HandlerData arch_set_handler(u32 i, IntHandler handler, void* userdata) {
	if (i >= 32) {
		used_ints[(i - 32) / 32] |= 1 << ((i - 32) % 32);
	}
	HandlerData old = handlers[i];
	handlers[i].handler = handler;
	handlers[i].userdata = userdata;
	return old;
}

void arch_dealloc_int(u32 i) {
	used_ints[(i - 32) / 32] &= ~(1 << ((i - 32) % 32));
	handlers[i].handler = NULL;
	handlers[i].userdata = NULL;
}

typedef struct {
	u64 r15, r14, r13, r12, r11, r10, r9, r8, rbp, rsi, rdi, rdx, rcx, rbx, rax;
	u64 vec;
	u64 error;
	u64 ip;
	u64 cs;
	u64 flags;
	u64 sp;
	u64 ss;
} InterruptCtx;

void x86_int_handler(InterruptCtx* ctx) {
	if (ctx->cs == 0x2b) {
		__asm__ volatile("swapgs");
	}

	HandlerData* data = &handlers[ctx->vec];

	if (data->handler) {
		data->handler(ctx, data->userdata);
	}
	else {
		kprintf("[kernel][x86]: unhandled interrupt %u\n", ctx->vec);
	}

	if (ctx->cs == 0x2b) {
		__asm__ volatile("swapgs");
	}
}