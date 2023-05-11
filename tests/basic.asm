default rel
global _start

%define SYS_EXIT 0
%define SYS_CREATE_THREAD 1
%define SYS_DPRINT 2
%define SYS_SLEEP 3
%define SYS_WAIT_THREAD 4

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
	xor edx, edx
	syscall

	push rax

	sub rsp, 16
	mov byte [rsp + 0], 'h'
	mov byte [rsp + 1], 'e'
	mov byte [rsp + 2], 'l'
	mov byte [rsp + 3], 'l'
	mov byte [rsp + 4], 'o'
	mov byte [rsp + 5], ' '
	mov byte [rsp + 6], 'f'
	mov byte [rsp + 7], 'r'
	mov byte [rsp + 8], 'o'
	mov byte [rsp + 9], 'm'
	mov byte [rsp + 10], ' '
	mov byte [rsp + 11], 'u'
	mov byte [rsp + 12], 's'
	mov byte [rsp + 13], 'e'
	mov byte [rsp + 14], 'r'
	mov byte [rsp + 15], 0xA
	mov edi, SYS_DPRINT
	mov rax, rsp
	mov rsi, 16
	syscall

	add rsp, 16

	mov edi, SYS_DPRINT
	lea rax, [str]
	mov rsi, str_len
	syscall

	pop rax
	mov edi, SYS_WAIT_THREAD
	syscall

	; exit
	mov edi, SYS_EXIT
	;xor eax, eax
	syscall

section .rodata
str: db "Hello world!", 0xA
str_len: equ $-str