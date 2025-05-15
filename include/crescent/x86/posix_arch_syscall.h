#ifndef CRESCENT_POSIX_ARCH_SYSCALL_H
#define CRESCENT_POSIX_ARCH_SYSCALL_H

typedef struct {
	long ret;
	long err;
} CrescentPosixSyscallResult;

static inline CrescentPosixSyscallResult __posix_syscall0(long num) {
	long ret;
	long err;
	// num
	// rdi
	__asm__ volatile("syscall" : "=a"(ret), "=D"(err) : "D"(num) : "rcx", "r11");
	return (CrescentPosixSyscallResult) {ret, err};
}

static inline CrescentPosixSyscallResult __posix_syscall1(long num, long a0) {
	long ret;
	long err;
	// num a0
	// rdi rsi
	__asm__ volatile("syscall" : "=a"(ret), "=D"(err) : "D"(num), "S"(a0) : "rcx", "r11");
	return (CrescentPosixSyscallResult) {ret, err};
}

static inline CrescentPosixSyscallResult __posix_syscall2(long num, long a0, long a1) {
	long ret;
	long err;
	// num a0  a1
	// rdi rsi rdx
	__asm__ volatile("syscall" : "=a"(ret), "=D"(err) : "D"(num), "S"(a0), "d"(a1) : "rcx", "r11");
	return (CrescentPosixSyscallResult) {ret, err};
}

static inline CrescentPosixSyscallResult __posix_syscall3(long num, long a0, long a1, long a2) {
	long ret;
	long err;
	// num a0  a1  a2
	// rdi rsi rdx rax
	__asm__ volatile("syscall" : "=a"(ret), "=D"(err) : "D"(num), "S"(a0), "d"(a1), "a"(a2) : "rcx", "r11");
	return (CrescentPosixSyscallResult) {ret, err};
}

static inline CrescentPosixSyscallResult __posix_syscall4(long num, long a0, long a1, long a2, long a3) {
	long ret;
	long err;
	// num a0  a1  a2  a3
	// rdi rsi rdx rax r8
	register long r8 __asm__("r8") = a3;
	__asm__ volatile("syscall" : "=a"(ret), "=D"(err) : "D"(num), "S"(a0), "d"(a1), "a"(a2), "r"(r8) : "rcx", "r11");
	return (CrescentPosixSyscallResult) {ret, err};
}

static inline CrescentPosixSyscallResult __posix_syscall5(long num, long a0, long a1, long a2, long a3, long a4) {
	long ret;
	long err;
	// num a0  a1  a2  a3 a4
	// rdi rsi rdx rax r8 r9
	register long r8 __asm__("r8") = a3;
	register long r9 __asm__("r9") = a4;
	__asm__ volatile("syscall" : "=a"(ret), "=D"(err) : "D"(num), "S"(a0), "d"(a1), "a"(a2), "r"(r8), "r"(r9) : "rcx", "r11");
	return (CrescentPosixSyscallResult) {ret, err};
}

static inline CrescentPosixSyscallResult __posix_syscall6(long num, long a0, long a1, long a2, long a3, long a4, long a5) {
	long ret;
	long err;
	// num a0  a1  a2  a3 a4 a5
	// rdi rsi rdx rax r8 r9 r10
	register long r8 __asm__("r8") = a3;
	register long r9 __asm__("r9") = a4;
	register long r10 __asm__("r10") = a5;
	__asm__ volatile("syscall" : "=a"(ret), "=D"(err) : "D"(num), "S"(a0), "d"(a1), "a"(a2), "r"(r8), "r"(r9), "r"(r10) : "rcx", "r11");
	return (CrescentPosixSyscallResult) {ret, err};
}

#endif
