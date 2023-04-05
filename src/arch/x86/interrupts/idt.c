#include "arch/interrupts.h"
#include "mem/pmalloc.h"
#include "mem/utils.h"
#include "string.h"

extern void* x86_int_stubs[];

typedef struct [[gnu::packed]] {
	u16 limit;
	u64 base;
} Idtr;

typedef struct {
	u16 offset0;
	u16 selector;
	u16 ist_flags;
	u16 offset1;
	u32 offset2;
	u32 reserved;
} IdtEntry;

static IdtEntry* idt;

#define X86_INT_GATE 0xE

static void set_idt_entry(u8 i, void* handler, u8 selector, u8 ist, u8 dpl) {
	usize addr = (usize) handler;
	idt[i].offset0 = addr;
	idt[i].offset1 = addr >> 16;
	idt[i].offset2 = addr >> 32;
	idt[i].selector = selector;
	idt[i].ist_flags = (ist & 0b111) | X86_INT_GATE << 8 | (dpl & 0b11) << 13 | 1 << 15;
}

void x86_init_idt() {
	Page* idt_page = pmalloc(1);
	assert(idt_page);
	idt = (IdtEntry*) to_virt(idt_page->phys);
	memset(idt, 0, PAGE_SIZE);

	for (u16 i = 0; i < 256; ++i) {
		set_idt_entry(i, x86_int_stubs[i], 0x8, 0, 0);
	}

	// NMI
	set_idt_entry(2, x86_int_stubs[2], 0x8, 2, 0);
	// DF
	set_idt_entry(8, x86_int_stubs[8], 0x8, 1, 0);
	// MCE
	set_idt_entry(18, x86_int_stubs[18], 0x8, 1, 0);

	Idtr idtr = {.limit = PAGE_SIZE - 1, .base = (u64) idt};
	__asm__ volatile("cli; lidt %0; sti" : : "m"(idtr));
}