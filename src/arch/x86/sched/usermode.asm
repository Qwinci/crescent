global x86_usermode_ret:function (x86_usermode_ret.end - x86_usermode_ret)
global x86_syscall_entry:function (x86_syscall_entry.end - x86_syscall_entry)
extern syscall_handler_count
extern syscall_handlers

struc X86Task
	.self: resq 1
	.rsp: resq 1
	.kernel_rsp: resq 1
	.syscall_save_rsp: resq 1
endstruc

x86_usermode_ret:
	swapgs
	pop rcx

	xor eax, eax
	xor edx, edx
	xor esi, esi
	xor edi, edi
	xor r8d, r8d
	xor r9d, r9d
	xor r10d, r10d

	mov r11, 0x202
	o64 sysret
.end:

x86_syscall_entry:
	swapgs
	mov qword [gs:X86Task.syscall_save_rsp], rsp
	mov rsp, [gs:X86Task.kernel_rsp]
	sti

	push r11
	push rcx

	cmp rdi, qword [syscall_handler_count]
	jl .continue
	mov rax, -1
	jmp .exit

.continue:
	; num arg0 arg1 arg2 arg3 arg4 arg5
	; rdi rax  rsi  rdx  r10  r8   r9
	mov rcx, r10

	mov r10, [syscall_handlers + (rdi * 8)]

	mov rdi, rax
	call r10

	cli
	xor edi, edi
	xor esi, esi
	xor edx, edx
	xor r8d, r8d
	xor r9d, r9d
	xor r10d, r10d

.exit:
	pop rcx
	pop r11

	mov rsp, [gs:X86Task.syscall_save_rsp]
	swapgs
	o64 sysret
.end: