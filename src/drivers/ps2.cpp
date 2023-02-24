#include "ps2.hpp"
#include "acpi/fadt.hpp"
#include "acpi/io_apic.hpp"
#include "arch.hpp"
#include "arch/x86/cpu.hpp"
#include "arch/x86/lapic.hpp"

static u8 ps2_kb_int = 0;
extern bool reboot;

static void ps2_int_handler(InterruptCtx* ctx) {
	println("ps2 int");
	fadt_reset();
	reboot = true;
	*(volatile u8*) nullptr = 0;
	Lapic::eoi();
}

void init_ps2() {
	if (ps2_kb_int == 0) {
		ps2_kb_int = alloc_int_handler(ps2_int_handler);
	}

	IoApic::RedirEntry ps2_kb_entry {
			.vector = ps2_kb_int,
			.dest = get_cpu_local()->id
	};

	IoApic::register_isa_irq(1, ps2_kb_entry);
}