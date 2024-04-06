#include "idt.hpp"
#include "types.hpp"
#include "arch/arch_irq.hpp"
#include "arch/irq.hpp"
#include "manually_destroy.hpp"

struct IdtEntry {
	u16 offset0;
	u16 selector;
	u16 ist_flags;
	u16 offset1;
	u32 offset2;
	u32 reserved;
};

struct [[gnu::packed]] Idtr {
	u16 limit;
	u64 base;
};

static IdtEntry IDT[256];

static void set_idt_entry(u8 i, void* handler, u8 selector, u8 ist, u8 dpl) {
	auto addr = reinterpret_cast<usize>(handler);
	auto& entry = IDT[i];
	entry.offset0 = addr;
	entry.offset1 = addr >> 16;
	entry.offset2 = addr >> 32;
	entry.selector = selector;
	entry.ist_flags = (ist & 0b111) | (0xE << 8) | (dpl & 0b11) << 13 | 1 << 15;
}

extern void* x86_irq_stubs[];

bool x86_div_handler(IrqFrame* frame);
bool x86_debug_handler(IrqFrame* frame);
bool x86_nmi_handler(IrqFrame* frame);
bool x86_breakpoint_handler(IrqFrame* frame);
bool x86_overflow_handler(IrqFrame* frame);
bool x86_bound_range_exceeded_handler(IrqFrame* frame);
bool x86_invalid_op_handler(IrqFrame* frame);
bool x86_device_not_available_handler(IrqFrame* frame);
bool x86_double_fault_handler(IrqFrame* frame);
bool x86_invalid_tss_handler(IrqFrame* frame);
bool x86_seg_not_present_handler(IrqFrame* frame);
bool x86_stack_seg_fault_handler(IrqFrame* frame);
bool x86_gp_fault_handler(IrqFrame* frame);

[[noreturn]] bool x86_pagefault_handler(IrqFrame* frame);
bool x86_x87_floating_point_handler(IrqFrame* frame);
bool x86_alignment_check_handler(IrqFrame* frame);
bool x86_machine_check_handler(IrqFrame* frame);
bool x86_simd_exception_handler(IrqFrame* frame);
bool x86_virt_exception_handler(IrqFrame* frame);
bool x86_control_protection_exception_handler(IrqFrame* frame);
bool x86_hypervisor_injection_exception_handler(IrqFrame* frame);
bool x86_vmm_comm_exception_handler(IrqFrame* frame);
bool x86_security_exception_handler(IrqFrame* frame);

static ManuallyDestroy<IrqHandler> EXCEPTION_HANDLERS[32] {
	{{.fn = x86_div_handler}},
	{{.fn = x86_debug_handler}},
	{{.fn = x86_nmi_handler}},
	{{.fn = x86_breakpoint_handler}},
	{{.fn = x86_overflow_handler}},
	{{.fn = x86_bound_range_exceeded_handler}},
	{{.fn = x86_invalid_op_handler}},
	{{.fn = x86_device_not_available_handler}},
	{{.fn = x86_double_fault_handler}},
	{},
	{{.fn = x86_invalid_tss_handler}},
	{{.fn = x86_seg_not_present_handler}},
	{{.fn = x86_stack_seg_fault_handler}},
	{{.fn = x86_gp_fault_handler}},
	{{.fn = x86_pagefault_handler}},
	{},
	{{.fn = x86_x87_floating_point_handler}},
	{{.fn = x86_alignment_check_handler}},
	{{.fn = x86_machine_check_handler}},
	{{.fn = x86_simd_exception_handler}},
	{{.fn = x86_virt_exception_handler}},
	{{.fn = x86_control_protection_exception_handler}},
	{}, {}, {}, {}, {}, {},
	{{.fn = x86_hypervisor_injection_exception_handler}},
	{{.fn = x86_vmm_comm_exception_handler}},
	{{.fn = x86_security_exception_handler}},
	{}
};

void x86_init_idt() {
	for (u32 i = 0; i < 256; ++i) {
		set_idt_entry(i, x86_irq_stubs[i], 0x8, 0, 0);
	}

	for (u32 i = 0; i < 32; ++i) {
		register_irq_handler(i, &*EXCEPTION_HANDLERS[i]);
	}

	x86_load_idt();
}

void x86_load_idt() {
	Idtr idtr {
		.limit = 0x1000 - 1,
		.base = reinterpret_cast<u64>(IDT)
	};
	asm volatile("cli; lidt %0; sti" : : "m"(idtr));
}
