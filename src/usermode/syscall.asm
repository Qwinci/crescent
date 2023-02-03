global syscall_entry_asm
extern syscall_handler_count
extern syscall_handlers

; rax = num, rdi = arg0, rsi = arg1, rdx = arg2, r8 = arg3, r9 = arg4

syscall_entry_asm:
	swapgs
	mov qword [gs:0x8], rsp
	mov rsp, [gs:0x10]

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
	mov rsp, [gs:0x8]
	swapgs
	o64 sysret