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

static IrqHandler IRQ_EXCEPTIONS[] = {
	[0] = {.fn = ex_div},
	[1] = {.fn = ex_debug},
	[2] = {.fn = ex_nmi},
	[3] = {.fn = ex_breakpoint},
	[4] = {.fn = ex_overflow},
	[5] = {.fn = ex_bound_range_exceeded},
	[6] = {.fn = ex_invalid_op},
	[7] = {.fn = ex_dev_not_available},
	[8] = {.fn = ex_df},
	[0xA] = {.fn = ex_invalid_tss},
	[0xB] = {.fn = ex_seg_not_present},
	[0xC] = {.fn = ex_stack_seg_fault},
	[0xD] = {.fn = ex_gp},
	[0xE] = {.fn = ex_pf},
	[0x10] = {.fn = ex_x86_float},
	[0x11] = {.fn = ex_align_check},
	[0x12] = {.fn = ex_mce},
	[0x13] = {.fn = ex_simd},
	[0x14] = {.fn = ex_virt},
	[0x15] = {.fn = ex_control_protection},
	[0x1C] = {.fn = ex_hypervisor},
	[0x1D] = {.fn = ex_vmm_comm},
	[0x1E] = {.fn = ex_security}
};

void x86_init_idt() {
	Page* idt_page = pmalloc(1);
	assert(idt_page);
	idt = (IdtEntry*) to_virt(idt_page->phys);
	memset(idt, 0, PAGE_SIZE);

	for (u16 i = 0; i < 256; ++i) {
		set_idt_entry(i, x86_int_stubs[i], 0x8, 0, 0);
	}

	arch_irq_install(0x0, &IRQ_EXCEPTIONS[0x0], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0x1, &IRQ_EXCEPTIONS[0x1], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0x2, &IRQ_EXCEPTIONS[0x2], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0x3, &IRQ_EXCEPTIONS[0x3], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0x4, &IRQ_EXCEPTIONS[0x4], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0x5, &IRQ_EXCEPTIONS[0x5], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0x6, &IRQ_EXCEPTIONS[0x6], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0x7, &IRQ_EXCEPTIONS[0x7], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0x8, &IRQ_EXCEPTIONS[0x8], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0xA, &IRQ_EXCEPTIONS[0xA], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0xB, &IRQ_EXCEPTIONS[0xB], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0xC, &IRQ_EXCEPTIONS[0xC], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0xD, &IRQ_EXCEPTIONS[0xD], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0xE, &IRQ_EXCEPTIONS[0xE], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0x10, &IRQ_EXCEPTIONS[0x10], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0x11, &IRQ_EXCEPTIONS[0x11], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0x12, &IRQ_EXCEPTIONS[0x12], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0x13, &IRQ_EXCEPTIONS[0x13], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0x14, &IRQ_EXCEPTIONS[0x14], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0x15, &IRQ_EXCEPTIONS[0x15], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0x1C, &IRQ_EXCEPTIONS[0x1C], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0x1D, &IRQ_EXCEPTIONS[0x1D], IRQ_INSTALL_FLAG_NONE);
	arch_irq_install(0x1E, &IRQ_EXCEPTIONS[0x1E], IRQ_INSTALL_FLAG_NONE);

	x86_load_idt();
}

void x86_load_idt() {
	Idtr idtr = {.limit = PAGE_SIZE - 1, .base = (u64) idt};
	__asm__ volatile("cli; lidt %0; sti" : : "m"(idtr));
}