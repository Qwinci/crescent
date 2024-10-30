.globl BSP_STACK
.comm BSP_STACK, 0x8000, 16

.section .text
.globl _start
.type _start, @function
_start:
	lea BSP_STACK + 0x8000 - 8(%rip), %rsp
	jmp early_start

.section .note.GNU-stack
