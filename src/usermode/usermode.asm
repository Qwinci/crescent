global enter_usermode
global usermode_ret

enter_usermode:
	mov rcx, rdi
	mov r11, 0x202
	o64 sysret

usermode_ret:
	swapgs
	pop rcx

	xor rax, rax
	xor rdx, rdx
	xor rsi, rsi
	xor rdi, rdi
	xor r8, r8
	xor r9, r9
	xor r10, r10

	mov r11, 0x202
	o64 sysret