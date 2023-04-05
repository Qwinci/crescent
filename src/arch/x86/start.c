#include "arch/x86/interrupts/gdt.h"
#include "interrupts/idt.h"
#include <stddef.h>

[[noreturn]] extern void kmain();

extern void x86_init_con();
extern void x86_init_mem();

[[noreturn, gnu::used]] void start() {
	x86_init_con();
	x86_init_mem();
	x86_load_gdt(NULL);
	x86_init_idt();
	kmain();
}