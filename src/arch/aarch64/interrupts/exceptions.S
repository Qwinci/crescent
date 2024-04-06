.section .text
.globl EXCEPTION_HANDLERS_START

.cfi_sections .debug_frame

.macro stub_exception_handler
	.cfi_startproc simple
	.cfi_signal_frame

	sub sp, sp, #(8 * 26)
	.cfi_def_cfa sp, 0
	stp x0, x1, [sp, #0]
	.cfi_offset x0, 0
	.cfi_offset x1, 8
	stp x2, x3, [sp, #16]
	.cfi_offset x2, 16
	.cfi_offset x3, 24
	stp x4, x5, [sp, #32]
	.cfi_offset x4, 32
	.cfi_offset x5, 40
	stp x6, x7, [sp, #48]
	.cfi_offset x6, 48
	.cfi_offset x7, 56
	stp x8, x9, [sp, #64]
	.cfi_offset x8, 64
	.cfi_offset x9, 72
	stp x10, x11, [sp, #80]
	.cfi_offset x10, 80
	.cfi_offset x11, 88
	stp x12, x13, [sp, #96]
	.cfi_offset x12, 96
	.cfi_offset x13, 104
	stp x14, x15, [sp, #112]
	.cfi_offset x14, 112
	.cfi_offset x15, 120
	stp x16, x17, [sp, #128]
	.cfi_offset x16, 128
	.cfi_offset x17, 136
	stp x18, x29, [sp, #144]
	.cfi_offset x18, 144
	.cfi_offset x29, 152
	mrs x0, ELR_EL1
	stp x30, x0, [sp, #160]
	.cfi_offset x30, 168

	mrs x0, ESR_EL1
	mrs x1, FAR_EL1
	stp x0, x1, [sp, #176]

	mrs x2, spsr_el1
	stp x2, xzr, [sp, #192]

	mov x0, sp
	bl arch_exception_handler

	ldp x0, xzr, [sp, #192]
	msr spsr_el1, x0

	ldr x0, [sp, #168]
	msr elr_el1, x0

	ldp x0, x1, [sp, #0]
	ldp x2, x3, [sp, #16]
	ldp x4, x5, [sp, #32]
	ldp x6, x7, [sp, #48]
	ldp x8, x9, [sp, #64]
	ldp x10, x11, [sp, #80]
	ldp x12, x13, [sp, #96]
	ldp x14, x15, [sp, #112]
	ldp x16, x17, [sp, #128]
	ldp x18, x29, [sp, #144]
	ldp x30, xzr, [sp, #160]

	add sp, sp, #(8 * 26)
	eret

.cfi_endproc
.endm

.balign 0x800
EXCEPTION_HANDLERS_START:
curr_el_sp0_sync:
stub_exception_handler

.balign 0x80
curr_el_sp0_irq:
stub_exception_handler

.balign 0x80
curr_el_sp0_fiq:
stub_exception_handler

.balign 0x80
curr_el_sp0_serror:
stub_exception_handler

.balign 0x80
curr_el_spx_sync: // current SP
stub_exception_handler

.balign 0x80
curr_el_spx_irq:
stub_exception_handler

.balign 0x80
curr_el_spx_fiq:
stub_exception_handler

.balign 0x80
curr_el_spx_serror:
stub_exception_handler

.balign 0x80
lower_el_aarch64_sync:
stub_exception_handler

.balign 0x80
lower_el_aarch64_irq:
stub_exception_handler

.balign 0x80
lower_el_aarch64_fiq:
stub_exception_handler

.balign 0x80
lower_el_aarch64_serror:
stub_exception_handler

.balign 0x80
lower_el_aarch32_sync:
stub_exception_handler

.balign 0x80
lower_el_aarch32_irq:
stub_exception_handler

.balign 0x80
lower_el_aarch32_fiq:
stub_exception_handler

.balign 0x80
lower_el_aarch32_serror:
stub_exception_handler
