#include "interrupts.hpp"
#include "console.hpp"
#include "utils/misc.hpp"

extern void* int_stubs[];

Idt* idt {};

Idt::Idt() {
	for (u16 i = 0; i < 256; ++i) {
		interrupts[i] = {int_stubs[i], 0x8, 0, false, 0};
	}
}

struct [[gnu::packed]] Idtr {
	u16 size;
	u64 offset;
};

static struct {
	Handler handler;
	void* arg;
} handlers[256] {};

extern "C" void int_handler(InterruptCtx* ctx) {
	if (ctx->cs == 0x2b) {
		swapgs();
	}

	auto& handler = handlers[ctx->vec];

	handler.handler(ctx, handler.arg);

	if (ctx->cs == 0x2b) {
		swapgs();
	}
}

void register_int_handler(u8 vec, Handler handler, void* arg) {
	handlers[vec].handler = handler;
	handlers[vec].arg = arg;
}

u16 alloc_int_handler(Handler handler, void* arg) {
	for (u16 i = 32; i < 256; ++i) {
		if (!handlers[i].handler) {
			handlers[i].handler = handler;
			handlers[i].arg = arg;
			return i;
		}
	}
	return 0;
}

Handler get_int_handler(u8 vec) {
	return handlers[vec].handler;
}

void load_idt() {
	if (!idt) {
		idt = new Idt();
	}
	asm volatile("cli");
	Idtr idtr {.size = 0x1000 - 1, .offset = cast<u64>(idt)};
	asm volatile("lidt %0" : : "m"(idtr));
	asm volatile("sti");
}

void set_exceptions() {
	handlers[0].handler = division_exception;
	handlers[1].handler = debug_exception;
	handlers[2].handler = nmi_exception;
	handlers[3].handler = breakpoint_exception;
	handlers[4].handler = overflow_exception;
	handlers[5].handler = bound_range_exception;
	handlers[6].handler = invalid_op_exception;
	handlers[7].handler = device_not_available_exception;
	handlers[8].handler = double_fault_exception;
	handlers[10].handler = invalid_tss_exception;
	handlers[11].handler = seg_not_present_exception;
	handlers[12].handler = stack_seg_fault_exception;
	handlers[13].handler = gp_fault_exception;
	handlers[14].handler = page_fault_exception;
	handlers[16].handler = x87_float_exception;
	handlers[17].handler = alignment_exception;
	handlers[18].handler = machine_check_exception;
	handlers[19].handler = simd_float_exception;
	handlers[20].handler = virtualization_exception;
	handlers[21].handler = control_protection_exception;
	handlers[28].handler = hypervisor_injection_exception;
	handlers[29].handler = vmm_communication_exception;
	handlers[30].handler = security_exception;
}