global _start

%define SYS_EXIT 0
%define SYS_CREATE_THREAD 1

thread:
	mov edi, SYS_EXIT
	mov eax, 1
	syscall
_start:
	; num arg0 arg1 arg2 arg3 arg4 arg5
	; rdi rax  rsi  rdx  r10  r8   r9

	; create a thread
	mov edi, SYS_CREATE_THREAD
	lea rax, [rel thread]
	xor esi, esi
	syscall

	; exit
	mov edi, SYS_EXIT
	xor eax, eax
	syscall
