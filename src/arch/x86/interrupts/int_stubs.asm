.global int_common
.type int_common, @function

.cfi_sections .debug_frame

.macro m_stub i
stub\i:
.cfi_startproc
.if \i != 8 && \i != 10 && \i != 11 && \i != 12 && \i != 13 && \i != 14 && \i != 17 && \i != 21 && \i != 29 && \i != 30
	push $0
.endif
    .cfi_def_cfa_offset 0
    .cfi_rel_offset %rip, 0x8
    .cfi_rel_offset %rsp, 0x20
	push $\i
	.cfi_adjust_cfa_offset 8
	cld
	jmp int_common
.cfi_endproc
.endm

.altmacro
.section .text
.set i, 0
.rept 256
m_stub %i
.set i, i + 1
.endr

int_common:
.cfi_startproc
    .cfi_def_cfa_offset 0
    .cfi_rel_offset %rip, 16
    .cfi_rel_offset %rsp, 0x20 + 8

	push %rax
	.cfi_adjust_cfa_offset 8
	push %rbx
	.cfi_adjust_cfa_offset 8
	push %rcx
	.cfi_adjust_cfa_offset 8
	push %rdx
	.cfi_adjust_cfa_offset 8
	push %rdi
	.cfi_adjust_cfa_offset 8
	push %rsi
	.cfi_adjust_cfa_offset 8
	push %rbp
	.cfi_adjust_cfa_offset 8
	push %r8
	.cfi_adjust_cfa_offset 8
	push %r9
	.cfi_adjust_cfa_offset 8
	push %r10
	.cfi_adjust_cfa_offset 8
	push %r11
	.cfi_adjust_cfa_offset 8
	push %r12
	.cfi_adjust_cfa_offset 8
	push %r13
	.cfi_adjust_cfa_offset 8
	push %r14
	.cfi_adjust_cfa_offset 8
	push %r15
	.cfi_adjust_cfa_offset 8

    mov %rsp, %r15

    pushq 17 * 8(%rsp)
    .cfi_adjust_cfa_offset 8
	//push qword [rsp + 17 * 8]
	push %rbp
	.cfi_adjust_cfa_offset 8
	mov %rsp, %rbp

    mov %r15, %rdi

    // align stack
    sub $8, %rsp
    .cfi_adjust_cfa_offset 8
	call x86_int_handler

    add $8 * 3, %rsp
    .cfi_adjust_cfa_offset -8 * 3

	pop %r15
	.cfi_adjust_cfa_offset -8
	pop %r14
	.cfi_adjust_cfa_offset -8
	pop %r13
	.cfi_adjust_cfa_offset -8
	pop %r12
	.cfi_adjust_cfa_offset -8
	pop %r11
	.cfi_adjust_cfa_offset -8
	pop %r10
	.cfi_adjust_cfa_offset -8
	pop %r9
	.cfi_adjust_cfa_offset -8
	pop %r8
	.cfi_adjust_cfa_offset -8
	pop %rbp
	.cfi_adjust_cfa_offset -8
	pop %rsi
	.cfi_adjust_cfa_offset -8
	pop %rdi
	.cfi_adjust_cfa_offset -8
	pop %rdx
	.cfi_adjust_cfa_offset -8
	pop %rcx
	.cfi_adjust_cfa_offset -8
	pop %rbx
	.cfi_adjust_cfa_offset -8
	pop %rax

    add $16, %rsp
	iretq
.cfi_endproc

.section .data
.macro m_label i
    .quad stub\i
.endm

.global x86_int_stubs
x86_int_stubs:
.set i, 0
.rept 256
m_label %i
.set i, i + 1
.endr
