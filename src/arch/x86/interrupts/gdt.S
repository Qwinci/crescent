.global x86_load_gdt_asm
.type x86_load_gdt_asm, @function

x86_load_gdt_asm:
    lgdt (%rdi)
    mov $0x10, %ax
	mov %ax, %ds
	mov %ax, %ss
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	pop %rdi
	push $0x8
	push %rdi
	lretq