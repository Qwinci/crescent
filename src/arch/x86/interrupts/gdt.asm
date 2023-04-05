global x86_load_gdt_asm

x86_load_gdt_asm:
	lgdt [rdi]
	mov ax, 0x10
	mov ds, ax
	mov ss, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	pop rdi
	push dword 0x8
	push rdi
	retfq