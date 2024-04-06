.globl x86_syscall_stub
.type x86_syscall_stub, @function
x86_syscall_stub:
	swapgs
	mov %rsp, %gs:24
	mov %gs:16, %rsp
	cld
	sti

	sub $80, %rsp
	mov %rax, 0(%rsp)
	mov %rcx, 8(%rsp)
	mov %rdx, 16(%rsp)
	mov %rdi, 24(%rsp)
	mov %rsi, 32(%rsp)
	mov %r8, 40(%rsp)
	mov %r9, 48(%rsp)
	mov %r10, 56(%rsp)
	mov %r11, 64(%rsp)

	mov %rsp, %rdi
	call syscall_handler

	mov 0(%rsp), %rax
	mov 8(%rsp), %rcx
	mov 16(%rsp), %rdx
	mov 24(%rsp), %rdi
	mov 32(%rsp), %rsi
	mov 40(%rsp), %r8
	mov 48(%rsp), %r9
	mov 56(%rsp), %r10
	mov 64(%rsp), %r11
	add $80, %rsp

	cli
	mov %gs:24, %rsp
	swapgs
	sysretq

.section .note.GNU-stack
