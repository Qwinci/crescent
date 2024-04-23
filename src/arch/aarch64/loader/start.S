.local STACK
.comm STACK, 0x8000, 16

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

	b early_start

.section .note.GNU-stack
