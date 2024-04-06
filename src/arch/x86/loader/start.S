.section .bss
.lcomm STACK, 0x8000

.section .text
.globl _start
.type _start, @function
_start:
	lea STACK + 0x8000(%rip), %rsp
	jmp early_start

.section .note.GNU-stack
