.extern kstart
.set STACK_LEN, 0x2000

.globl start
.type start, @function

.cfi_sections .debug_frame

start:
    .cfi_startproc
    .cfi_def_cfa_register %rbp
    leaq STACK + STACK_LEN (%rip), %rsp
    xor %ebp, %ebp
    call kstart
    .cfi_endproc
.section .bss
.lcomm STACK, STACK_LEN
