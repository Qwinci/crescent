.globl mem_copy_to_user
.globl mem_copy_to_kernel

.type mem_copy_to_user, @function
mem_copy_to_user:
	push %rbp
	mov %rsp, %rbp

	cmp HHDM_START(%rip), %rdi
	jae 1f
	lea (%rdi, %rdx, 1), %rcx
	cmp HHDM_START(%rip), %rcx
	ja 1f

	lea 1f(%rip), %rax
	mov %rax, %gs:32
	mov %rsp, %gs:40

	cmpb $0, CPU_FEATURES + 14(%rip)
	je unsafe_copy

	stac
	call memcpy
	clac

	xor %edi, %edi
	mov %rdi, %gs:32

	pop %rbp
	mov $1, %eax
	ret

1:
	xor %eax, %eax
	mov %rax, %gs:32
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

	lea 1f(%rip), %rax
	mov %rax, %gs:32
	mov %rsp, %gs:40

	cmpb $0, CPU_FEATURES + 14(%rip)
	je unsafe_copy

	stac
	call memcpy
	clac

	xor %edi, %edi
	mov %rdi, %gs:32

	pop %rbp
	mov $1, %eax
	ret

1:
	xor %eax, %eax
	mov %rax, %gs:32
	pop %rbp
	ret

unsafe_copy:
	call memcpy
	xor %edi, %edi
	mov %rdi, %gs:32

	pop %rbp
	mov $1, %eax
	ret

.section .note.GNU-stack
