#pragma once
#include <stddef.h>

/*
num a0  a1  a2  a3  a4  a5
rdi rsi rdx rcx r8  r9  stack
rdi rsi rdx r10 r8  r9  rax
*/

static inline size_t syscall0(size_t num) {
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num) : "r11", "rcx");
	return ret;
}

static inline size_t syscall1(size_t num, size_t a0) {
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num), "S"(a0) : "r11", "rcx");
	return ret;
}

static inline size_t syscall2(size_t num, size_t a0, size_t a1) {
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num), "S"(a0), "d"(a1) : "r11", "rcx");
	return ret;
}

static inline size_t syscall3(size_t num, size_t a0, size_t a1, size_t a2) {
	register size_t r10 __asm__("r10") = a2;
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num), "S"(a0), "d"(a1), "r"(r10) : "r11", "rcx");
	return ret;
}

static inline size_t syscall4(size_t num, size_t a0, size_t a1, size_t a2, size_t a3) {
	register size_t r10 __asm__("r10") = a2;
	register size_t r8 __asm__("r8") = a3;
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num), "S"(a0), "d"(a1), "r"(r10), "r"(r8) : "r11", "rcx");
	return ret;
}

static inline size_t syscall5(size_t num, size_t a0, size_t a1, size_t a2, size_t a3, size_t a4) {
	register size_t r10 __asm__("r10") = a2;
	register size_t r8 __asm__("r8") = a3;
	register size_t r9 __asm__("r9") = a4;
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num), "S"(a0), "d"(a1), "r"(r10), "r"(r8), "r"(r9) : "r11", "rcx");
	return ret;
}

static inline size_t syscall6(size_t num, size_t a0, size_t a1, size_t a2, size_t a3, size_t a4, size_t a5) {
	register size_t r10 __asm__("r10") = a2;
	register size_t r8 __asm__("r8") = a3;
	register size_t r9 __asm__("r9") = a4;
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num), "S"(a0), "d"(a1), "r"(r10), "r"(r8), "r"(r9), "a"(a5) : "r11", "rcx");
	return ret;
}
