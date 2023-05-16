#include "acpi/acpi.h"
#include "arch/cpu.h"
#include "arch/x86/acpi/fadt.h"
#include "arch/x86/acpi/madt.h"
#include "arch/x86/interrupts/gdt.h"
#include "interrupts/idt.h"
#include "limine/limine.h"
#include "stdio.h"
#include <stddef.h>

extern void kmain();

extern void x86_init_con();
extern void x86_init_mem();
extern void debug_init();

u8 stack[0x2000] = {};

[[noreturn, gnu::naked, gnu::used]] void start() {
	__asm__ volatile("lea rsp, [rip + %0]; xor rbp, rbp; jmp kstart" : : "i"(stack + 0x2000));
}

static volatile struct limine_rsdp_request RSDP_REQUEST = {
	.id = LIMINE_RSDP_REQUEST
};

[[gnu::used, noreturn]] void kstart() {
	x86_init_con();
	debug_init();
	x86_init_mem();
	x86_load_gdt(NULL);
	x86_init_idt();
	if (RSDP_REQUEST.response) {
		acpi_init(RSDP_REQUEST.response->address);
	}
	arch_init_smp();

	if (RSDP_REQUEST.response) {
		x86_parse_madt();
		x86_parse_fadt();
	}

	kmain();
	panic("kstart: entered unreachable code\n");
}