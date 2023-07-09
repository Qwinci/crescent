.global _start
.set SYS_EXIT, 0
.set SYS_CREATE_THREAD, 1
.set SYS_DPRINT, 2
.set SYS_SLEEP, 3
.set SYS_WAIT_THREAD, 4

thread:
    mov $SYS_EXIT, %edi
	mov $1, %eax
	syscall
_start:
	// num arg0 arg1 arg2 arg3 arg4 arg5
	// rdi rax  rsi  rdx  r10  r8   r9

	// create a thread
	mov $SYS_CREATE_THREAD, %edi
	leaq thread(%rip), %rax
	xor %esi, %esi
	xor %edx, %edx
	syscall

	push %rax

    sub %rsp, 16
	movb $'h', 0(%rsp)
	movb $'e', 1(%rsp)
	movb $'l', 2(%rsp)
	movb $'l', 3(%rsp)
	movb $'o', 4(%rsp)
	movb $' ', 5(%rsp)
	movb $'f', 6(%rsp)
	movb $'r', 7(%rsp)
	movb $'o', 8(%rsp)
	movb $'m', 9(%rsp)
	movb $' ', 10(%rsp)
	movb $'u', 11(%rsp)
	movb $'s', 12(%rsp)
	movb $'e', 13(%rsp)
	movb $'r', 14(%rsp)
	movb $0xA, 15(%rsp)
	mov $SYS_DPRINT, %edi
	mov %rsp, %rax
	mov $16, %esi
	syscall

    add $16, %rsp

    mov $SYS_DPRINT, %edi
    leaq str(%rip), %rax
    mov $str_len, %rsi
	syscall

	pop %rax
	mov $SYS_WAIT_THREAD, %edi
	syscall

	// exit
	mov $SYS_EXIT, %edi
	// xor eax, eax
	syscall

.section .rodata
str: .ascii "Hello world!\n"
.set str_len, . - str