extern int_handler

section .text
%assign i 0
%rep 256
stub%+i:
%if i != 8 && i != 10 && i != 11 && i != 12 && i != 13 && i != 14 && i != 17 && i != 21 && i != 29 && i != 30
	push 0
%endif
	push i
	jmp int_common
%assign i i + 1
%endrep

struc InterruptCtx
	.regs: resq 15
	.vec: resq 1
	.error: resq 1
	.ip: resq 1
	.cs: resq 1
	.flags: resq 1
	.sp: resq 1
	.ss: resq 1
endstruc

int_common:
	push rax
	push rbx
	push rcx
	push rdx
	push rdi
	push rsi
	push rbp
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15

	; save the original rsp
	mov r15, rsp
	cld

	mov rdi, r15
	push qword [rdi + InterruptCtx.ip]
    and rsp, ~0xF
	mov rbp, rsp

	call int_handler
	mov rsp, r15

	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rbp
	pop rsi
	pop rdi
	pop rdx
	pop rcx
	pop rbx
	pop rax

	add rsp, 16

	iretq

section .data
global int_stubs
int_stubs:
%assign i 0
%rep 256
	dq stub%+i
%assign i i + 1
%endrep