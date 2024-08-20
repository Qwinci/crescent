.section .text
.globl EXCEPTION_HANDLERS_START

.cfi_sections .debug_frame

.macro stub_exception_handler name el
\name:
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
	str x30, [sp, #160]

.if \el == 0
	mrs x0, sp_el0
	str x0, [sp, #168]
.endif

	mrs x0, esr_el1
	mrs x1, far_el1
	mrs x2, elr_el1
	mrs x3, spsr_el1

	stp x0, x1, [sp, #176]
	stp x2, x3, [sp, #192]
	.cfi_offset x30, 192
	.cfi_offset pstate, 200

	mov x0, sp
	mov x29, xzr
	bl arch_exception_handler

	ldp x0, x1, [sp, #176]
	ldp x2, x3, [sp, #192]

	msr esr_el1, x0
	msr far_el1, x1
	msr elr_el1, x2
	msr spsr_el1, x3

.if \el == 0
	ldr x4, [sp, #168]
	msr sp_el0, x4
.endif

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

stub_exception_handler curr_el_sp0_sync 1
stub_exception_handler curr_el_sp0_irq 1
stub_exception_handler curr_el_sp0_fiq 1
stub_exception_handler curr_el_sp0_serror 1

stub_exception_handler curr_el_spx_sync 1
stub_exception_handler curr_el_spx_irq 1
stub_exception_handler curr_el_spx_fiq 1
stub_exception_handler curr_el_spx_serror 1

stub_exception_handler lower_el_aarch64_sync 0
stub_exception_handler lower_el_aarch64_irq 0
stub_exception_handler lower_el_aarch64_fiq 0
stub_exception_handler lower_el_aarch64_serror 0

stub_exception_handler lower_el_aarch32_sync 0
stub_exception_handler lower_el_aarch32_irq 0
stub_exception_handler lower_el_aarch32_fiq 0
stub_exception_handler lower_el_aarch32_serror 0

.balign 0x800
EXCEPTION_HANDLERS_START:
b curr_el_sp0_sync

.balign 0x80
b curr_el_sp0_irq

.balign 0x80
b curr_el_sp0_fiq

.balign 0x80
b curr_el_sp0_serror

.balign 0x80
b curr_el_spx_sync

.balign 0x80
b curr_el_spx_irq

.balign 0x80
b curr_el_spx_fiq

.balign 0x80
b curr_el_spx_serror

.balign 0x80
b lower_el_aarch64_sync

.balign 0x80
b lower_el_aarch64_irq

.balign 0x80
b lower_el_aarch64_fiq

.balign 0x80
b lower_el_aarch64_serror

.balign 0x80
b lower_el_aarch32_sync

.balign 0x80
b lower_el_aarch32_irq

.balign 0x80
b lower_el_aarch32_fiq

.balign 0x80
b lower_el_aarch32_serror
