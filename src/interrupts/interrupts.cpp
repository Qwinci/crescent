#include "interrupts.hpp"

extern void* int_stubs[];

Idt::Idt() {
	for (u16 i = 0; i < 32; ++i) {
		interrupts[i] = {int_stubs[i], 0x8, 0, true, 0};
	}
	for (u16 i = 32; i < 256; ++i) {
		interrupts[i] = {int_stubs[i], 0x8, 0, false, 0};
	}
}

struct [[gnu::packed]] Idtr {
	u16 size;
	u64 offset;
};

static Handler handlers[256] {};

extern "C" void int_handler(InterruptCtx* ctx) {
	handlers[ctx->vec](ctx);
}

void register_int_handler(u8 vec, Handler handler) {
	handlers[vec] = handler;
}

u16 alloc_int_handler(Handler handler) {
	for (u16 i = 32; i < 256; ++i) {
		if (!handlers[i]) {
			handlers[i] = handler;
			return i;
		}
	}
	return 0;
}

Handler get_int_handler(u8 vec) {
	return handlers[vec];
}

void load_idt(Idt* idt) {
	asm volatile("cli");
	Idtr idtr {.size = 0x1000 - 1, .offset = cast<u64>(idt)};
	asm volatile("lidt %0" : : "m"(idtr));
	asm volatile("sti");
}

void set_exceptions() {
	handlers[0] = division_exception;
	handlers[1] = debug_exception;
	handlers[2] = nmi_exception;
	handlers[3] = breakpoint_exception;
	handlers[4] = overflow_exception;
	handlers[5] = bound_range_exception;
	handlers[6] = invalid_op_exception;
	handlers[7] = device_not_available_exception;
	handlers[8] = double_fault_exception;
	handlers[10] = invalid_tss_exception;
	handlers[11] = seg_not_present_exception;
	handlers[12] = stack_seg_fault_exception;
	handlers[13] = gp_fault_exception;
	handlers[14] = page_fault_exception;
	handlers[16] = x87_float_exception;
	handlers[17] = alignment_exception;
	handlers[18] = machine_check_exception;
	handlers[19] = simd_float_exception;
	handlers[20] = virtualization_exception;
	handlers[21] = control_protection_exception;
	handlers[28] = hypervisor_injection_exception;
	handlers[29] = vmm_communication_exception;
	handlers[30] = security_exception;
}