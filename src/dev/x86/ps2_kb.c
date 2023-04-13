#include "ps2_kb.h"
#include "arch/interrupts.h"
#include "arch/x86/dev/io.h"
#include "arch/x86/dev/io_apic.h"
#include "ps2.h"
#include "stdio.h"

static bool ps2_kb_handler(void*, void*) {
	u8 bytes[2] = {ps2_data_read()};
	io_wait();
	if (ps2_status_read() & PS2_STATUS_OUTPUT_FULL) {
		bytes[1] = ps2_data_read();
	}

	kprintf("ps2 data: %03u %03u\r", bytes[0], bytes[1]);

	return true;
}

extern u8 X86_BSP_ID;

void ps2_kb_init(bool second) {
	u8 i = arch_alloc_int(ps2_kb_handler, NULL);
	if (!i) {
		kprintf("[kernel][x86]: failed to allocate interrupt for ps2 keyboard\n");
		return;
	}
	IoApicRedirEntry entry = {
		.vec = i,
		.dest = X86_BSP_ID
	};
	if (!second) {
		io_apic_register_isa_irq(1, entry);
	}
	else {
		io_apic_register_isa_irq(12, entry);
	}
}