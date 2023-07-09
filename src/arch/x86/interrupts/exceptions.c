#include "arch/misc.h"
#include "arch/x86/cpu.h"
#include "interrupts.h"
#include "stdio.h"
#include "arch/x86/dev/lapic.h"
#include "arch/interrupts.h"

#define GENERIC_EX(ex_name, msg) \
NORETURN IrqStatus ex_##ex_name(void* void_ctx, void*) {\
	InterruptCtx* ctx = (InterruptCtx*) void_ctx; \
	spinlock_lock(&PRINT_LOCK); \
	kprintf_nolock("%fg%s\n%s", COLOR_RED, msg, "backtrace:\n"); \
    backtrace_display(false); \
	spinlock_unlock(&PRINT_LOCK); \
	if (ctx->cs == 0x2b) { \
		kprintf("killing user task '%s'", arch_get_cur_task()->name); \
		sched_kill_cur(); \
	} \
	\
	lapic_ipi_all(LAPIC_MSG_PANIC);\
	\
	while (true) { \
		arch_hlt(); \
	} \
}

GENERIC_EX(div, "division by zero")
GENERIC_EX(debug, "debug exception")
GENERIC_EX(nmi, "nmi exception")
GENERIC_EX(breakpoint, "breakpoint")
GENERIC_EX(overflow, "overflow")
GENERIC_EX(bound_range_exceeded, "bound range exceeded")
GENERIC_EX(invalid_op, "invalid opcode")
GENERIC_EX(dev_not_available, "device not available")
GENERIC_EX(df, "double fault")
GENERIC_EX(invalid_tss, "invalid tss")
GENERIC_EX(seg_not_present, "segment not present")
GENERIC_EX(stack_seg_fault, "stack-segment fault")
GENERIC_EX(gp, "general protection fault")
GENERIC_EX(x86_float, "x86 floating point exception")
GENERIC_EX(align_check, "alignment check exception")
GENERIC_EX(mce, "machine check exception")
GENERIC_EX(simd, "simd floating point exception")
GENERIC_EX(virt, "virtualization exception")
GENERIC_EX(control_protection, "control protection exception")
GENERIC_EX(hypervisor, "hypervisor injection exception")
GENERIC_EX(vmm_comm, "vmm communication exception")
GENERIC_EX(security, "security exception")

NORETURN void ex_pf(void* void_ctx, void*) {
	InterruptCtx* ctx = (InterruptCtx*) void_ctx;

	bool present = ctx->error & 1;
	bool write = ctx->error & 1 << 1;
	bool user = ctx->error & 1 << 2;
	bool reserved_write = ctx->error & 1 << 3;
	bool inst_fetch = ctx->error & 1 << 4;
	bool pk = ctx->error & 1 << 5;
	bool ss = ctx->error & 1 << 6;
	bool sgx = ctx->error & 1 << 15;

	u64 addr;
	__asm__ volatile("mov %%cr2, %0" : "=r"(addr));

	spinlock_lock(&PRINT_LOCK);
	kprintf_nolock("%fgpage fault caused by ", COLOR_RED);
	if (user) kputs_nolock("userspace ", sizeof("userspace ") - 1);
	if (write) kprintf_nolock("write to 0x%x at ip 0x%p\n", addr, ctx->ip);
	else kprintf_nolock("read from 0x%x at ip 0x%p\n", addr, ctx->ip);

	const char* reason = NULL;
	if (reserved_write) reason = "page entry contains reserved bits";
	else if (inst_fetch) reason = "on instruction fetch";
	else if (pk) reason = "protection key violation";
	else if (ss) reason = "shadow stack access";
	else if (sgx) reason = "sgx violation";
	else if (!present) reason = "non present page";
	if (reason) kprintf_nolock("(%s)\n", reason);

	kputs_nolock("backtrace:\n", sizeof("backtrace:\n") - 1);
	backtrace_display(false);
	spinlock_unlock(&PRINT_LOCK);

	if (ctx->cs == 0x2b) {
		kprintf("killing user task '%s'", arch_get_cur_task()->name);
		sched_kill_cur();
	}

	lapic_ipi_all(LAPIC_MSG_PANIC);

	while (true) {
		arch_hlt();
	}
}