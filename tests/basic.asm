global start

start:
	; num arg0 arg1 arg2 arg3 arg4 arg5
	; rdi rax  rsi  rdx  r10  r8   r9
	mov edi, 0
	syscall
