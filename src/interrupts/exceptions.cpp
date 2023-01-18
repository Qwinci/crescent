#include "exceptions.hpp"
#include "console.hpp"
#include "interrupts.hpp"

#define GENERIC_FAULT(Name) { \
	set_fg(0xFF0000); \
	println(#Name); \
	println("IP: ", Fmt::Hex, frame->ip); \
	while (true) asm("hlt"); \
}

[[noreturn, gnu::interrupt]] void double_fault_exception(InterruptFrame* frame, u64) {
	set_fg(0xFF0000);
	println("double fault");
	println("IP: ", Fmt::Hex, frame->ip);
	while (true) asm("hlt");
}

[[noreturn, gnu::interrupt]] void gp_fault_exception(InterruptFrame* frame, u64) {
	set_fg(0xFF0000);
	println("general protection fault");
	println("IP: ", Fmt::Hex, frame->ip);
	while (true) asm("hlt");
}

[[noreturn, gnu::interrupt]] void page_fault_exception(InterruptFrame* frame, u64 code) {
	u64 addr;
	asm volatile("mov %0, cr2" : "=r"(addr));

	bool present = code & 1;
	bool write = code & 1 << 1;
	bool user = code & 1 << 2;
	bool reserved_write = code & 1 << 3;
	bool inst_fetch = code & 1 << 4;
	/*bool protection_key = code & 1 << 5;
	bool shadow_stack = code & 1 << 6;
	bool software_guard_extensions = code & 1 << 15;*/

	set_fg(0xFF0000);
	println("page fault at address ", Fmt::Hex, addr);
	println("IP: ", frame->ip);
	println(
			"Present: ", present,
			", Write: ", write,
			", User: ", user,
			", Reserved Write: ", reserved_write,
			", Instruction Fetch: ", inst_fetch
			);
	while (true) asm("hlt");
}


[[noreturn, gnu::interrupt]] void division_exception(InterruptFrame* frame) GENERIC_FAULT(division error)
[[noreturn, gnu::interrupt]] void debug_exception(InterruptFrame* frame) GENERIC_FAULT(debug exception)
[[noreturn, gnu::interrupt]] void nmi_exception(InterruptFrame* frame) GENERIC_FAULT(nmi exception)
[[noreturn, gnu::interrupt]] void breakpoint_exception(InterruptFrame* frame) GENERIC_FAULT(breakpoint exception)
[[noreturn, gnu::interrupt]] void overflow_exception(InterruptFrame* frame) GENERIC_FAULT(overflow exception)
[[noreturn, gnu::interrupt]] void bound_range_exception(InterruptFrame* frame) GENERIC_FAULT(bound range Exceeded)
[[noreturn, gnu::interrupt]] void invalid_op_exception(InterruptFrame* frame) GENERIC_FAULT(invalid opcode)
[[noreturn, gnu::interrupt]] void device_not_available_exception(InterruptFrame* frame) GENERIC_FAULT(device not available)
[[noreturn, gnu::interrupt]] void invalid_tss_exception(InterruptFrame* frame, u64) GENERIC_FAULT(invalid tss)
[[noreturn, gnu::interrupt]] void seg_not_present_exception(InterruptFrame* frame, u64) GENERIC_FAULT(segment not present)
[[noreturn, gnu::interrupt]] void stack_seg_fault_exception(InterruptFrame* frame, u64) GENERIC_FAULT(stack segmentation fault)
[[noreturn, gnu::interrupt]] void x87_float_exception(InterruptFrame* frame) GENERIC_FAULT(x86 floating point exception)
[[noreturn, gnu::interrupt]] void alignment_exception(InterruptFrame* frame, u64) GENERIC_FAULT(alignment check)
[[noreturn, gnu::interrupt]] void machine_check_exception(InterruptFrame* frame) GENERIC_FAULT(machine check)
[[noreturn, gnu::interrupt]] void simd_float_exception(InterruptFrame* frame) GENERIC_FAULT(simd floating point exception)
[[noreturn, gnu::interrupt]] void virtualization_exception(InterruptFrame* frame) GENERIC_FAULT(virtualization exception)
[[noreturn, gnu::interrupt]] void control_protection_exception(InterruptFrame* frame, u64) GENERIC_FAULT(control protection exception)
[[noreturn, gnu::interrupt]] void hypervisor_injection_exception(InterruptFrame* frame) GENERIC_FAULT(hypervisor injection exception)
[[noreturn, gnu::interrupt]] void vmm_communication_exception(InterruptFrame* frame, u64) GENERIC_FAULT(vmm communication exception)
[[noreturn, gnu::interrupt]] void security_exception(InterruptFrame* frame, u64) GENERIC_FAULT(security exception)
