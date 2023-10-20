// bool arch_mem_copy_to_user(__user void* restrict user, const void* restrict kernel, usize size);
// bool arch_mem_copy_to_kernel(void* restrict kernel, __user const void* restrict user, usize size);

#include <arch/x86/sched/usermode.inc>

.globl arch_mem_copy_to_user
.globl arch_mem_copy_to_kernel

.type arch_mem_copy_to_user, @function
arch_mem_copy_to_user:
	push %rbp
	mov %rsp, %rbp

	cmp %rdi, HHDM_OFFSET(%rip)
	jbe 1f

	lea 1f(%rip), %rax
	mov %rax, %gs:X86TASK_HANDLER_IP_OFF
	mov %rsp, %gs:X86TASK_HANDLER_SP_OFF

	call memcpy

	xor %edi, %edi
	mov %rdi, %gs:X86TASK_HANDLER_IP_OFF

	pop %rbp
	mov $1, %eax
	ret
1:
	xor %eax, %eax
	mov %rax, %gs:X86TASK_HANDLER_IP_OFF
	pop %rbp
	ret

.type arch_mem_copy_to_kernel, @function
arch_mem_copy_to_kernel:
	push %rbp
	mov %rsp, %rbp

	cmp %rsi, HHDM_OFFSET(%rip)
	jbe 1f

	lea 1f(%rip), %rax
	mov %rax, %gs:X86TASK_HANDLER_IP_OFF
	mov %rsp, %gs:X86TASK_HANDLER_SP_OFF

	call memcpy

	xor %edi, %edi
	mov %rdi, %gs:X86TASK_HANDLER_IP_OFF

	pop %rbp
	mov $1, %eax
	ret
1:
	xor %eax, %eax
	mov %rax, %gs:X86TASK_HANDLER_IP_OFF
	pop %rbp
	ret