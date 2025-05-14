.local STACK
.comm STACK, 0x8000, 16

#define DT_RELA 7
#define DT_RELASZ 8

#define R_AARCH64_RELATIVE 1027

.section .text.boot
.globl asm_start
asm_start:
	b asm_real // branch to kernel start, magic
	.long 0x00 // reserved
	.quad 0x00 // Image load offset from start of RAM, unused
	.quad 0x00 // reserved
	.quad 0x00 // reserved
	.quad 0x00 // reserved
	.quad 0x00 // reserved
	.quad 0x00 // reserved
	.byte 0x41 // Magic number, "ARM\x64"
	.byte 0x52
	.byte 0x4d
	.byte 0x64
	.word 0x00 // reserved

asm_real:
	// zero out bss
	adrp x2, BSS_START
	add x2, x2, :lo12:BSS_START
	adrp x3, BSS_END
	add x3, x3, :lo12:BSS_END
	loop:
		cmp x2, x3
		b.eq end

		strb wzr, [x2], #1
		b loop
	end:

	adrp x1, STACK
	add x1, x1, :lo12:STACK
	add x1, x1, #0x8000
	mov sp, x1

	mov x19, x0

	adrp x0, KERNEL_START
	add x0, x0, :lo12:KERNEL_START
	mov x1, x0
	bl self_relocate

	mov x0, x19

	bl early_start

	adrp x1, KERNEL_START
	add x1, x1, :lo12:KERNEL_START
	sub sp, sp, x1
	add sp, sp, x0

	mov x20, x0
	mov x21, x1

	bl self_relocate

	adrp x2, arch_start
	add x2, x2, :lo12:arch_start
	sub x2, x2, x21
	add x2, x2, x20

	mov x0, x19
	mov x1, x21

	br x2

// void self_relocate(usize virt_base, usize phys_base)
self_relocate:
	adrp x2, _DYNAMIC
	add x2, x2, :lo12:_DYNAMIC

	mov x6, 0xFFFFFFFF80000000

	mov x4, 0
	mov x5, 0

dyn_loop:
	ldr x3, [x2]
	cbz x3, dyn_end

	cmp x3, DT_RELA
	bne 0f
	ldr x4, [x2, #8]
	b increment
0:
	cmp x3, DT_RELASZ
	bne increment
	ldr x5, [x2, #8]

increment:
	add x2, x2, 16
	b dyn_loop
dyn_end:

	// - link base
	sub x4, x4, x6
	// + physical base
	add x4, x4, x1
	// rela end
	add x5, x4, x5

reloc_loop:
	cmp x4, x5
	beq reloc_end

	// r_type
	ldr w2, [x4, #8]
	cmp w2, R_AARCH64_RELATIVE
	beq 0f
	udf #0
0:
	// r_offset
	ldr x2, [x4]
	// - link base
	sub x2, x2, x6
	// + phys_base
	add x2, x2, x1

	// r_addend
	ldr x3, [x4, #16]
	// - link base
	sub x3, x3, x6
	// + base
	add x3, x3, x0

	str x3, [x2]

	add x4, x4, 24
	b reloc_loop
reloc_end:
	ret

.section .note.GNU-stack
