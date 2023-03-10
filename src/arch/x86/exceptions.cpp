#include "exceptions.hpp"
#include "arch.hpp"
#include "console.hpp"
#include "cpu.hpp"
#include "debug/backtrace.hpp"
#include "interrupts/interrupts.hpp"
#include "lapic.hpp"

bool full_int_init = false;
bool reboot = false;

#define GENERIC_FAULT(Name) { \
	print_lock.lock(); \
	set_fg(0xFF0000); \
	if (full_int_init) println_nolock(#Name, " on cpu ", get_cpu_local()->id); \
    else println_nolock(#Name, " on unknown cpu (not init yet)");\
	println_nolock("IP: ", Fmt::Hex, ctx->ip, Fmt::Dec); \
	debug_backtrace(false); \
	while (true) asm("hlt"); \
}

[[noreturn]] void double_fault_exception(InterruptCtx* ctx, void*) {
	if (reboot) {
		println("double fault (reboot)");
		*(volatile u8*) nullptr = 0;
	}

	print_lock.lock();
	set_fg(0xFF0000);

	if (full_int_init) println_nolock("double fault on cpu ", get_cpu_local()->id);
	else println_nolock("double fault on unknown cpu (not init yet)");
	println_nolock("IP: ", Fmt::Hex, ctx->ip, Fmt::Dec);

	debug_backtrace(false);

	print_lock.unlock();

	while (true) asm("hlt");
}

[[noreturn]] void gp_fault_exception(InterruptCtx* ctx, void*) {
	print_lock.lock();
	set_fg(0xFF0000);

	if (full_int_init) println_nolock("general protection fault on cpu ", get_cpu_local()->id);
	else println_nolock("general protection fault on unknown cpu (not init yet)");
	println_nolock("IP: ", Fmt::Hex, ctx->ip, Fmt::Dec);

	debug_backtrace(false);

	print_lock.unlock();

	while (true) asm("hlt");
}

[[noreturn]] void page_fault_exception(InterruptCtx* ctx, void*) {
	if (reboot) {
		println("page fault (reboot)");
		*(volatile u8*) nullptr = 0;
	}

	u64 addr;
	asm volatile("mov %0, cr2" : "=r"(addr));

	auto code = ctx->error;
	bool present = code & 1;
	bool write = code & 1 << 1;
	bool user = code & 1 << 2;
	bool reserved_write = code & 1 << 3;
	bool inst_fetch = code & 1 << 4;
	/*bool protection_key = code & 1 << 5;
	bool shadow_stack = code & 1 << 6;
	bool software_guard_extensions = code & 1 << 15;*/

	disable_interrupts();
	print_lock.lock();
	set_fg(0xFF0000);
	if (full_int_init) println_nolock("page fault at address ", Fmt::Hex, addr, Fmt::Dec, " on cpu ", get_cpu_local()->id);
	else println_nolock("page fault at address ", Fmt::Hex, addr, Fmt::Dec, " on unknown cpu (not init yet)");
	println_nolock(Fmt::Hex, "IP: ", ctx->ip, Fmt::Dec);
	println_nolock(
			"Present: ", present,
			", Write: ", write,
			", User: ", user,
			", Reserved Write: ", reserved_write,
			", Instruction Fetch: ", inst_fetch
			);

	debug_backtrace(false);

	if (user) {
		println_nolock("USER TASK '", as<const char*>(arch_get_cpu_local()->current_task->name),
		        "' IS GOING TO BE TERMINATED BECAUSE IT DID STUPID STUFF", Fmt::Dec);
		set_fg(0x00FF00);
		print_lock.unlock();
		sched_kill();
	}
	else {
		Lapic::send_ipi_all(Lapic::Msg::Halt);
		print_lock.unlock();
	}

	while (true) asm("hlt");
}

[[noreturn]] void division_exception(InterruptCtx* ctx, void*) GENERIC_FAULT(division error)
[[noreturn]] void debug_exception(InterruptCtx* ctx, void*) GENERIC_FAULT(debug exception)
[[noreturn]] void nmi_exception(InterruptCtx* ctx, void*) GENERIC_FAULT(nmi exception)
[[noreturn]] void breakpoint_exception(InterruptCtx* ctx, void*) GENERIC_FAULT(breakpoint exception)
[[noreturn]] void overflow_exception(InterruptCtx* ctx, void*) GENERIC_FAULT(overflow exception)
[[noreturn]] void bound_range_exception(InterruptCtx* ctx, void*) GENERIC_FAULT(bound range Exceeded)
[[noreturn]] void invalid_op_exception(InterruptCtx* ctx, void*) GENERIC_FAULT(invalid opcode)
[[noreturn]] void device_not_available_exception(InterruptCtx* ctx, void*) GENERIC_FAULT(device not available)
[[noreturn]] void invalid_tss_exception(InterruptCtx* ctx, void*) GENERIC_FAULT(invalid tss)
[[noreturn]] void seg_not_present_exception(InterruptCtx* ctx, void*) GENERIC_FAULT(segment not present)
[[noreturn]] void stack_seg_fault_exception(InterruptCtx* ctx, void*) GENERIC_FAULT(stack segmentation fault)
[[noreturn]] void x87_float_exception(InterruptCtx* ctx, void*) GENERIC_FAULT(x86 floating point exception)
[[noreturn]] void alignment_exception(InterruptCtx* ctx, void*) GENERIC_FAULT(alignment check)
[[noreturn]] void machine_check_exception(InterruptCtx* ctx, void*) GENERIC_FAULT(machine check)
[[noreturn]] void simd_float_exception(InterruptCtx* ctx, void*) GENERIC_FAULT(simd floating point exception)
[[noreturn]] void virtualization_exception(InterruptCtx* ctx, void*) GENERIC_FAULT(virtualization exception)
[[noreturn]] void control_protection_exception(InterruptCtx* ctx, void*) GENERIC_FAULT(control protection exception)
[[noreturn]] void hypervisor_injection_exception(InterruptCtx* ctx, void*) GENERIC_FAULT(hypervisor injection exception)
[[noreturn]] void vmm_communication_exception(InterruptCtx* ctx, void*) GENERIC_FAULT(vmm communication exception)
[[noreturn]] void security_exception(InterruptCtx* ctx, void*) GENERIC_FAULT(security exception)
