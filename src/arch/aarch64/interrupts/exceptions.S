.section .text
.globl EXCEPTION_HANDLERS_START

#define KERNEL_SP_OFF 8

.cfi_sections .debug_frame

.macro stub_exception_handler name el handler
\name:
	.cfi_startproc simple
	.cfi_signal_frame

	msr tpidrro_el0, x0
.if \el == 0
	mrs x0, tpidr_el1
	ldr x0, [x0, KERNEL_SP_OFF]
.else
	mov x0, sp
.endif

	sub x0, x0, #288
	.cfi_def_cfa x0, 0

	str x1, [x0, #8]
	.cfi_offset x1, 8
	stp x2, x3, [x0, #16]
	.cfi_offset x2, 16
	.cfi_offset x3, 24
	stp x4, x5, [x0, #32]
	.cfi_offset x4, 32
	.cfi_offset x5, 40
	stp x6, x7, [x0, #48]
	.cfi_offset x6, 48
	.cfi_offset x7, 56
	stp x8, x9, [x0, #64]
	.cfi_offset x8, 64
	.cfi_offset x9, 72
	stp x10, x11, [x0, #80]
	.cfi_offset x10, 80
	.cfi_offset x11, 88
	stp x12, x13, [x0, #96]
	.cfi_offset x12, 96
	.cfi_offset x13, 104
	stp x14, x15, [x0, #112]
	.cfi_offset x14, 112
	.cfi_offset x15, 120
	stp x16, x17, [x0, #128]
	.cfi_offset x16, 128
	.cfi_offset x17, 136
	stp x18, x19, [x0, #144]
	.cfi_offset x18, 144
	.cfi_offset x19, 152
	stp x20, x21, [x0, #160]
	.cfi_offset x20, 160
	.cfi_offset x21, 168
	stp x22, x23, [x0, #176]
	.cfi_offset x22, 176
	.cfi_offset x23, 184
	stp x24, x25, [x0, #192]
	.cfi_offset x24, 192
	.cfi_offset x25, 200
	stp x26, x27, [x0, #208]
	.cfi_offset x26, 208
	.cfi_offset x27, 216
	stp x28, x29, [x0, #224]
	.cfi_offset x28, 224
	.cfi_offset x29, 232
	str x30, [x0, #240]

.if \el == 0
	mrs x1, sp_el0
.else
	mov x1, sp
.endif
	str x1, [x0, #248]
	.cfi_offset sp, 248

	mov sp, x0
	.cfi_def_cfa_register sp

	mrs x0, tpidrro_el0
	str x0, [sp, #0]
	.cfi_offset x0, 0

	msr tpidrro_el0, xzr

	mrs x0, esr_el1
	mrs x1, far_el1
	mrs x2, elr_el1
	mrs x3, spsr_el1

	stp x0, x1, [sp, #256]
	stp x2, x3, [sp, #272]
	.cfi_offset x30, 272
	.cfi_offset pstate, 280

	mov x0, sp
	mov x29, xzr
	bl \handler

	ldp x0, x1, [sp, #256]
	ldp x2, x3, [sp, #272]

	msr esr_el1, x0
	msr far_el1, x1
	msr elr_el1, x2
	msr spsr_el1, x3

.if \el == 0
	ldr x4, [sp, #248]
	msr sp_el0, x4
.endif

	ldp x2, x3, [sp, #16]
	ldp x4, x5, [sp, #32]
	ldp x6, x7, [sp, #48]
	ldp x8, x9, [sp, #64]
	ldp x10, x11, [sp, #80]
	ldp x12, x13, [sp, #96]
	ldp x14, x15, [sp, #112]
	ldp x16, x17, [sp, #128]
	ldp x18, x19, [sp, #144]
	ldp x20, x21, [sp, #160]
	ldp x22, x23, [sp, #176]
	ldp x24, x25, [sp, #192]
	ldp x26, x27, [sp, #208]
	ldp x28, x29, [sp, #224]
	ldr x30, [sp, #240]

	mov x0, sp
	.cfi_def_cfa_register x0

.if \el == 1
	ldr x1, [x0, #248]
	mov sp, x1
.endif

	ldp x0, x1, [x0, #0]

	eret

.cfi_endproc
.endm

stub_exception_handler curr_el_sp0_sync 1 arch_exception_handler
stub_exception_handler curr_el_sp0_irq 1 arch_irq_handler
stub_exception_handler curr_el_sp0_fiq 1 arch_exception_handler
stub_exception_handler curr_el_sp0_serror 1 arch_exception_handler

stub_exception_handler curr_el_spx_sync 1 arch_exception_handler
stub_exception_handler curr_el_spx_irq 1 arch_irq_handler
stub_exception_handler curr_el_spx_fiq 1 arch_exception_handler
stub_exception_handler curr_el_spx_serror 1 arch_exception_handler

stub_exception_handler lower_el_aarch64_sync 0 arch_exception_handler
stub_exception_handler lower_el_aarch64_irq 0 arch_irq_handler
stub_exception_handler lower_el_aarch64_fiq 0 arch_exception_handler
stub_exception_handler lower_el_aarch64_serror 0 arch_exception_handler

stub_exception_handler lower_el_aarch32_sync 0 arch_exception_handler
stub_exception_handler lower_el_aarch32_irq 0 arch_irq_handler
stub_exception_handler lower_el_aarch32_fiq 0 arch_exception_handler
stub_exception_handler lower_el_aarch32_serror 0 arch_exception_handler

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
