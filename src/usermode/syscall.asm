global syscall_entry_asm
extern syscall_handler_count
extern syscall_handlers
extern current_task

%include "sched/task.inc"

; rax = num, rdi = arg0, rsi = arg1, rdx = arg2, r8 = arg3, r9 = arg4

syscall_entry_asm:
	swapgs
	push rbp
	mov rbp, [current_task]
	mov qword [rbp + Task.rsp], rsp
	mov rsp, [rbp + Task.kernel_rsp]

	push r11
	push rcx

	cmp rax, qword [syscall_handler_count]
	jl .continue
	mov rax, -1
	jmp .exit

.continue:
	mov rax, [syscall_handlers + (rax * 8)]

	mov rcx, r8
	mov r8, r9

	call rax

	xor rdi, rdi
	xor rsi, rsi
	xor rdx, rdx
	xor r8, r8
	xor r9, r9
	xor r10, r10

.exit:
	pop rcx
	pop r11

	mov rsp, [rbp + Task.rsp]
	pop rbp
	swapgs
	o64 sysret