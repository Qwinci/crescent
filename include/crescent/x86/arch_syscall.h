#ifndef CRESCENT_ARCH_SYSCALL_H
#define CRESCENT_ARCH_SYSCALL_H

static inline long __syscall0(long num) {
	long ret;
	// num
	// rdi
	asm volatile("syscall" : "=a"(ret) : "D"(num) : "rcx", "r11");
	return ret;
}

static inline long __syscall1(long num, long a0) {
	long ret;
	// num a0
	// rdi rsi
	asm volatile("syscall" : "=a"(ret) : "D"(num), "S"(a0) : "rcx", "r11");
	return ret;
}

static inline long __syscall2(long num, long a0, long a1) {
	long ret;
	// num a0  a1
	// rdi rsi rdx
	asm volatile("syscall" : "=a"(ret) : "D"(num), "S"(a0), "d"(a1) : "rcx", "r11");
	return ret;
}

static inline long __syscall3(long num, long a0, long a1, long a2) {
	long ret;
	// num a0  a1  a2
	// rdi rsi rdx rax
	asm volatile("syscall" : "=a"(ret) : "D"(num), "S"(a0), "d"(a1), "a"(a2) : "rcx", "r11");
	return ret;
}

static inline long __syscall4(long num, long a0, long a1, long a2, long a3) {
	long ret;
	// num a0  a1  a2  a3
	// rdi rsi rdx rax r8
	register long r8 asm("r8") = a3;
	asm volatile("syscall" : "=a"(ret) : "D"(num), "S"(a0), "d"(a1), "a"(a2), "r"(r8) : "rcx", "r11");
	return ret;
}

static inline long __syscall5(long num, long a0, long a1, long a2, long a3, long a4) {
	long ret;
	// num a0  a1  a2  a3 a4
	// rdi rsi rdx rax r8 r9
	register long r8 asm("r8") = a3;
	register long r9 asm("r9") = a4;
	asm volatile("syscall" : "=a"(ret) : "D"(num), "S"(a0), "d"(a1), "a"(a2), "r"(r8), "r"(r9) : "rcx", "r11");
	return ret;
}

static inline long __syscall6(long num, long a0, long a1, long a2, long a3, long a4, long a5) {
	long ret;
	// num a0  a1  a2  a3 a4 a5
	// rdi rsi rdx rax r8 r9 r10
	register long r8 asm("r8") = a3;
	register long r9 asm("r9") = a4;
	register long r10 asm("r10") = a5;
	asm volatile("syscall" : "=a"(ret) : "D"(num), "S"(a0), "d"(a1), "a"(a2), "r"(r8), "r"(r9), "r"(r10) : "rcx", "r11");
	return ret;
}

#endif
