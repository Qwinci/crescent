.globl x86_syscall_stub
.type x86_syscall_stub, @function
x86_syscall_stub:
	swapgs
	mov %rsp, %gs:24
	mov %gs:16, %rsp
	cld
	sti

	sub $128, %rsp
	mov %rax, 0(%rsp)
	mov %rbx, 8(%rsp)
	mov %rcx, 16(%rsp)
	mov %rdx, 24(%rsp)
	mov %rdi, 32(%rsp)
	mov %rsi, 40(%rsp)
	mov %rbp, 48(%rsp)
	mov %r8, 56(%rsp)
	mov %r9, 64(%rsp)
	mov %r10, 72(%rsp)
	mov %r11, 80(%rsp)
	mov %r12, 88(%rsp)
	mov %r13, 96(%rsp)
	mov %r14, 104(%rsp)
	mov %r15, 112(%rsp)

	mov %rsp, %rdi
	call syscall_handler

	mov 0(%rsp), %rax
	mov 8(%rsp), %rbx
	mov 16(%rsp), %rcx
	mov 24(%rsp), %rdx
	mov 32(%rsp), %rdi
	mov 40(%rsp), %rsi
	mov 48(%rsp), %rbp
	mov 56(%rsp), %r8
	mov 64(%rsp), %r9
	mov 72(%rsp), %r10
	mov 80(%rsp), %r11
	mov 88(%rsp), %r12
	mov 96(%rsp), %r13
	mov 104(%rsp), %r14
	mov 112(%rsp), %r15
	add $128, %rsp

	cli
	mov %gs:24, %rsp
	swapgs
	sysretq

.section .note.GNU-stack
