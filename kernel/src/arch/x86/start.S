.global start
.type start, @function

.cfi_sections .debug_frame
.set STACK_SIZE, 0x2000

.section .bss
.lcomm STACK, STACK_SIZE

.section .text
start:
.cfi_startproc
    leaq STACK + STACK_SIZE(%rip), %rsp
    call kstart
.cfi_endproc

.section .note.GNU-stack
