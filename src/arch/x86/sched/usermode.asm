.global x86_usermode_ret
.global x86_syscall_entry
.type x86_usermode_ret, @function
.type x86_syscall_entry, @function

.set X86Task, 0
.set X86Task.self, 0
.set X86Task.rsp, 8
.set X86Task.kernel_rsp, X86Task.rsp + 8
.set X86Task.syscall_save_rsp, X86Task.kernel_rsp + 8

x86_usermode_ret:
	swapgs
	pop %rcx
	pop %rdi

	xor %eax, %eax
	xor %edx, %edx
	xor %esi, %esi
	xor %r8d, %r8d
	xor %r9d, %r9d
	xor %r10d, %r10d
	xor %r12d, %r12d
	xor %r13d, %r13d
	xor %r14d, %r14d
	xor %r15d, %r15d

	mov $0x202, %r11
	sysretq

x86_syscall_entry:
	swapgs
	mov %rsp, %gs:X86Task.syscall_save_rsp
	mov %gs:X86Task.kernel_rsp, %rsp
	sti

    push %rbx
    push %rcx
    push %rdx
    push %rdi
    push %rsi
    push %r8
    push %r9
    push %r10
	push %r11
	push %r12
	push %r13
	push %r14
	push %r15

    cmp (syscall_handler_count), %rdi
	jl 0f
	mov $-1, %rax
	jmp exit

0:
	// num arg0 arg1 arg2 arg3 arg4 arg5
	// rdi rax  rsi  rdx  r10  r8   r9

    mov %r10, %rcx
    mov syscall_handlers(, %rdi, 8), %r10

    mov %rax, %rdi
	call *%r10

	cli

exit:
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rsi
    pop %rdi
    pop %rdx
    pop %rcx
    pop %rbx

    mov %gs:X86Task.syscall_save_rsp, %rsp
	swapgs
	sysretq
