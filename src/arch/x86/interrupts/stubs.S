
.cfi_sections .debug_frame

.macro stub i

irq\i:
.cfi_startproc
.cfi_signal_frame
.cfi_def_cfa %rsp, 0
.cfi_offset %rip, 0
.cfi_offset %rsp, 24

.if \i != 8 && \i != 10 && \i != 11 && \i != 12 && \i != 13 && \i != 14 && \i != 17 && \i != 21 && \i != 29 && \i != 30
	push $0
.endif
.cfi_adjust_cfa_offset 8

cld

push %rax
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %rax, 0
push %rbx
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %rbx, 0
push %rcx
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %rcx, 0
push %rdx
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %rdx, 0
push %rdi
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %rdi, 0
push %rsi
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %rsi, 0
push %rbp
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %rbp, 0
push %r8
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %r8, 0
push %r9
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %r9, 0
push %r10
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %r10, 0
push %r11
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %r11, 0
push %r12
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %r12, 0
push %r13
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %r13, 0
push %r14
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %r14, 0
push %r15
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %r15, 0

mov %rsp, %rdi

pushq 17 * 8(%rsp)
.cfi_adjust_cfa_offset 8
push %rbp
.cfi_adjust_cfa_offset 8
mov %rsp, %rbp

mov $\i, %esi

call arch_irq_handler

add $16, %rsp
.cfi_adjust_cfa_offset -16

pop %r15
.cfi_adjust_cfa_offset -8
.cfi_restore %r15
pop %r14
.cfi_adjust_cfa_offset -8
.cfi_restore %r14
pop %r13
.cfi_adjust_cfa_offset -8
.cfi_restore %r13
pop %r12
.cfi_adjust_cfa_offset -8
.cfi_restore %r12
pop %r11
.cfi_adjust_cfa_offset -8
.cfi_restore %r11
pop %r10
.cfi_adjust_cfa_offset -8
.cfi_restore %r10
pop %r9
.cfi_adjust_cfa_offset -8
.cfi_restore %r9
pop %r8
.cfi_adjust_cfa_offset -8
.cfi_restore %r8
pop %rbp
.cfi_adjust_cfa_offset -8
.cfi_restore %rbp
pop %rsi
.cfi_adjust_cfa_offset -8
.cfi_restore %rsi
pop %rdi
.cfi_adjust_cfa_offset -8
.cfi_restore %rdi
pop %rdx
.cfi_adjust_cfa_offset -8
.cfi_restore %rdx
pop %rcx
.cfi_adjust_cfa_offset -8
.cfi_restore %rcx
pop %rbx
.cfi_adjust_cfa_offset -8
.cfi_restore %rbx
pop %rax
.cfi_adjust_cfa_offset -8
.cfi_restore %rax

add $8, %rsp
.cfi_adjust_cfa_offset -8
iretq

.cfi_endproc
.endm

.altmacro
.set i, 0
.rept 256
stub %i
.set i, i + 1
.endr

.macro label i
	.quad irq\i
.endm

.section .rodata
.global x86_irq_stubs
x86_irq_stubs:
.set i, 0
.rept 256
label %i
.set i, i + 1
.endr

.section .note.GNU-stack
