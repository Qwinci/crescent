.globl mem_copy_to_user
.globl mem_copy_to_kernel

mem_copy_safe:
	lea 1f(%rip), %rax
	mov %rax, %gs:32

	stac
	call memcpy
	clac

	movq $0, %gs:32

	pop %rbp
	mov $1, %eax
	ret
1:
	clac
	movq $0, %gs:32
	xor %eax, %eax
	pop %rbp
	ret

mem_copy_unsafe:
	lea 1f(%rip), %rax
	mov %rax, %gs:32

	call memcpy

	movq $0, %gs:32

	pop %rbp
	mov $1, %eax
	ret
1:
	movq $0, %gs:32
	xor %eax, %eax
	pop %rbp
	ret

.type mem_copy_to_user, @function
mem_copy_to_user:
	push %rbp
	mov %rsp, %rbp

	cmp HHDM_START(%rip), %rdi
	jae 1f
	lea (%rdi, %rdx, 1), %rcx
	cmp HHDM_START(%rip), %rcx
	ja 1f

	mov %rsp, %gs:40

	cmpb $0, CPU_FEATURES + 14(%rip)
	je mem_copy_unsafe
	jmp mem_copy_safe
1:
	xor %eax, %eax
	pop %rbp
	ret

.type mem_copy_to_kernel, @function
mem_copy_to_kernel:
	push %rbp
	mov %rsp, %rbp

	cmp HHDM_START(%rip), %rsi
	jae 1f
	lea (%rsi, %rdx, 1), %rcx
	cmp HHDM_START(%rip), %rcx
	ja 1f

	mov %rsp, %gs:40

	cmpb $0, CPU_FEATURES + 14(%rip)
	je mem_copy_unsafe
	jmp mem_copy_safe
1:
	xor %eax, %eax
	pop %rbp
	ret

.section .note.GNU-stack
