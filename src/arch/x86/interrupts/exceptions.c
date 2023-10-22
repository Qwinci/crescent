#include "arch/misc.h"
#include "arch/x86/cpu.h"
#include "interrupts.h"
#include "stdio.h"
#include "arch/x86/dev/lapic.h"
#include "arch/interrupts.h"
#include "exceptions.h"

extern void* x86_syscall_entry_exit[];

#define GENERIC_EX(ex_name, msg) \
IrqStatus ex_##ex_name(void* void_ctx, void*) {\
	InterruptCtx* ctx = (InterruptCtx*) void_ctx; \
	spinlock_lock(&PRINT_LOCK); \
	kprintf_nolock("%fg%s\n%s", COLOR_RED, msg, "backtrace:\n"); \
    backtrace_display(false); \
	spinlock_unlock(&PRINT_LOCK); \
    Task* task = arch_get_cur_task(); \
	if (ctx->cs == 0x2b) { \
		kprintf("killing user task '%s'", arch_get_cur_task()->name); \
		__asm__ volatile("sti"); \
		sched_kill_cur(); \
	} \
	else { \
		X86Task* x86_task = container_of(task, X86Task, common); \
		ctx->ip = (u64) x86_syscall_entry_exit; \
		ctx->sp = (u64) x86_task->kernel_stack_base + KERNEL_STACK_SIZE - 14 * 8; \
		ctx->rax = 0xFA10ED; \
		return IRQ_ACK; \
    } \
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

IrqStatus ex_pf(void* void_ctx, void*) {
	InterruptCtx* ctx = (InterruptCtx*) void_ctx;

	Task* self = arch_get_cur_task();

	u64 addr;
	__asm__ volatile("mov %%cr2, %0" : "=r"(addr));

	if (process_handle_fault(self->process, addr)) {
		return IRQ_ACK;
	}

	if (self->handler_ip) {
		ctx->ip = (usize) self->handler_ip;
		ctx->sp = (usize) self->handler_sp;
		self->handler_ip = 0;
		return IRQ_ACK;
	}

	bool present = ctx->error & 1;
	bool write = ctx->error & 1 << 1;
	bool user = ctx->error & 1 << 2;
	bool reserved_write = ctx->error & 1 << 3;
	bool inst_fetch = ctx->error & 1 << 4;
	bool pk = ctx->error & 1 << 5;
	bool ss = ctx->error & 1 << 6;
	bool sgx = ctx->error & 1 << 15;

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

	kprintf_nolock("rax: %X rbx: %X rcx: %X rdx: %X rdi: %X rsi: %X rsp: %X rbp: %X\n"
				   "r8: %X r9: %X r10: %X r11: %X r12: %X r13: %X r14: %X r15: %X\n",
				   ctx->rax, ctx->rbx, ctx->rcx, ctx->rdx, ctx->rdi, ctx->rsi, ctx->sp, ctx->rbp,
				   ctx->r8, ctx->r9, ctx->r10, ctx->r11, ctx->r12, ctx->r13, ctx->r14, ctx->r15);

	kputs_nolock("backtrace:\n", sizeof("backtrace:\n") - 1);
	backtrace_display(false);
	spinlock_unlock(&PRINT_LOCK);

	if (ctx->cs == 0x2b) {
		kprintf("killing user task '%s'\n", self->name);
		__asm__ volatile("sti");
		sched_kill_cur();
	}

	lapic_ipi_all(LAPIC_MSG_PANIC);

	while (true) {
		arch_hlt();
	}
}