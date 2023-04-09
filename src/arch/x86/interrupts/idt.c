#include "arch/interrupts.h"
#include "mem/pmalloc.h"
#include "mem/utils.h"
#include "string.h"
#include "idt.h"
#include "exceptions.h"

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

	arch_set_handler(0x0, (IntHandler) ex_div, NULL);
	arch_set_handler(0x1, (IntHandler) ex_debug, NULL);
	arch_set_handler(0x2, (IntHandler) ex_nmi, NULL);
	arch_set_handler(0x3, (IntHandler) ex_breakpoint, NULL);
	arch_set_handler(0x4, (IntHandler) ex_overflow, NULL);
	arch_set_handler(0x5, (IntHandler) ex_bound_range_exceeded, NULL);
	arch_set_handler(0x6, (IntHandler) ex_invalid_op, NULL);
	arch_set_handler(0x7, (IntHandler) ex_dev_not_available, NULL);
	arch_set_handler(0x8, (IntHandler) ex_df, NULL);
	arch_set_handler(0xA, (IntHandler) ex_invalid_tss, NULL);
	arch_set_handler(0xB, (IntHandler) ex_seg_not_present, NULL);
	arch_set_handler(0xC, (IntHandler) ex_stack_seg_fault, NULL);
	arch_set_handler(0xD, (IntHandler) ex_gp, NULL);
	arch_set_handler(0xE, (IntHandler) ex_pf, NULL);
	arch_set_handler(0x10, (IntHandler) ex_x86_float, NULL);
	arch_set_handler(0x11, (IntHandler) ex_align_check, NULL);
	arch_set_handler(0x12, (IntHandler) ex_mce, NULL);
	arch_set_handler(0x13, (IntHandler) ex_simd, NULL);
	arch_set_handler(0x14, (IntHandler) ex_virt, NULL);
	arch_set_handler(0x15, (IntHandler) ex_control_protection, NULL);
	arch_set_handler(0x1C, (IntHandler) ex_hypervisor, NULL);
	arch_set_handler(0x1D, (IntHandler) ex_vmm_comm, NULL);
	arch_set_handler(0x1E, (IntHandler) ex_security, NULL);

	x86_load_idt();
}

void x86_load_idt() {
	Idtr idtr = {.limit = PAGE_SIZE - 1, .base = (u64) idt};
	__asm__ volatile("cli; lidt %0; sti" : : "m"(idtr));
}