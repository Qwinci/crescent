#pragma once
#include "types.hpp"
#include "utils.hpp"
#include "exceptions.hpp"

struct InterruptCtx {
	u64 rax, rbx, rcx, rdx, rdi, rsi, rbp, r8, r9, r10, r11, r12, r13, r14, r15;
	u64 vec;
	u64 error;
	u64 ip;
	u64 cs;
	u64 flags;
	u64 sp;
	u64 ss;
};

struct InterruptFrame {
	u64 ip;
	u64 cs;
	u64 flags;
	u64 sp;
	u64 ss;
};

struct IdtEntry {
	constexpr IdtEntry() = default;
	constexpr IdtEntry(void* fn, u8 selector, u8 ist, bool trap, u8 dpl)
		: selector {selector}, ist {ist} {
		auto addr = cast<usize>(fn);
		offset0 = addr & 0xFFFF;
		offset1 = addr >> 16;
		offset2 = addr >> 32;
		flags = (trap ? 0xF : 0xE) | dpl << 5 | 1 << 7;
	}

	u16 offset0 {};
	u16 selector {};
	u8 ist {};
	u8 flags {};
	u16 offset1 {};
	u32 offset2 {};
	u32 reserved {};
};

struct Idt {
	IdtEntry division_error{cast<void*>(division_exception), 0x8, 0, true, 0};
	IdtEntry debug {cast<void*>(debug_exception), 0x8, 0, true, 0};
	IdtEntry nmi {cast<void*>(nmi_exception), 0x8, 0, true, 0};
	IdtEntry breakpoint {cast<void*>(breakpoint_exception), 0x8, 0, true, 0};
	IdtEntry overflow {cast<void*>(overflow_exception), 0x8, 0, true, 0};
	IdtEntry bound_range_exceeded {cast<void*>(bound_range_exception), 0x8, 0, true, 0};
	IdtEntry invalid_op {cast<void*>(invalid_op_exception), 0x8, 0, true, 0};
	IdtEntry device_not_available {cast<void*>(device_not_available_exception), 0x8, 0, true, 0};
	IdtEntry double_fault {cast<void*>(double_fault_exception), 0x8, 0, true, 0};
private:
	IdtEntry reserved1;
public:
	IdtEntry invalid_tss {cast<void*>(invalid_tss_exception), 0x8, 0, true, 0};
	IdtEntry seg_not_present {cast<void*>(seg_not_present_exception), 0x8, 0, true, 0};
	IdtEntry stack_seg_fault {cast<void*>(stack_seg_fault_exception), 0x8, 0, true, 0};
	IdtEntry gp_fault {cast<void*>(gp_fault_exception), 0x8, 0, true, 0};
	IdtEntry page_fault {cast<void*>(page_fault_exception), 0x8, 0, true, 0};
private:
	IdtEntry reserved2;
public:
	IdtEntry x87_float {cast<void*>(x87_float_exception), 0x8, 0, true, 0};
	IdtEntry alignment_check {cast<void*>(alignment_exception), 0x8, 0, true, 0};
	IdtEntry machine_check {cast<void*>(machine_check_exception), 0x8, 0, true, 0};
	IdtEntry simd_float {cast<void*>(simd_float_exception), 0x8, 0, true, 0};
	IdtEntry virtualization {cast<void*>(virtualization_exception), 0x8, 0, true, 0};
	IdtEntry control_protection {cast<void*>(control_protection_exception), 0x8, 0, true, 0};
private:
	IdtEntry reserved3[6];
public:
	IdtEntry hypervisor_injection {cast<void*>(hypervisor_injection_exception), 0x8, 0, true, 0};
	IdtEntry vmm_communication {cast<void*>(vmm_communication_exception), 0x8, 0, true, 0};
	IdtEntry security {cast<void*>(security_exception), 0x8, 0, true, 0};
private:
	IdtEntry reserved4;
public:
	IdtEntry interrupts[256 - 32];
};

extern Idt* current_idt;

template<typename T>
static inline u8 register_irq_handler(u8 irq, T fn, u8 ist = 0) {
	current_idt->interrupts[irq] = {(void*) fn, 0x8, ist, false, 0};
	return 32 + irq;
}

void load_idt(Idt* idt);

static inline void enable_interrupts() {
	asm volatile("sti");
}

static inline void disable_interrupts() {
	asm volatile("sti");
}