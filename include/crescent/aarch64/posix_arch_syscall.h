#ifndef CRESCENT_POSIX_ARCH_SYSCALL_H
#define CRESCENT_POSIX_ARCH_SYSCALL_H

typedef struct {
	long ret;
	long err;
} CrescentPosixSyscallResult;

static inline CrescentPosixSyscallResult __posix_syscall0(long num) {
	register long ret asm("x0");
	register long err asm("x1");
	register long x0 asm("x0") = num;
	asm volatile("svc #0" : "=r"(ret), "=r"(err) : "r"(x0));
	return (CrescentPosixSyscallResult) {ret, err};
}

static inline CrescentPosixSyscallResult __posix_syscall1(long num, long a0) {
	register long ret asm("x0");
	register long err asm("x1");
	register long x0 asm("x0") = num;
	register long x1 asm("x1") = a0;
	asm volatile("svc #0" : "=r"(ret), "=r"(err) : "r"(x0), "r"(x1));
	return (CrescentPosixSyscallResult) {ret, err};
}

static inline CrescentPosixSyscallResult __posix_syscall2(long num, long a0, long a1) {
	register long ret asm("x0");
	register long err asm("x1");
	register long x0 asm("x0") = num;
	register long x1 asm("x1") = a0;
	register long x2 asm("x2") = a1;
	asm volatile("svc #0" : "=r"(ret), "=r"(err) : "r"(x0), "r"(x1), "r"(x2));
	return (CrescentPosixSyscallResult) {ret, err};
}

static inline CrescentPosixSyscallResult __posix_syscall3(long num, long a0, long a1, long a2) {
	register long ret asm("x0");
	register long err asm("x1");
	register long x0 asm("x0") = num;
	register long x1 asm("x1") = a0;
	register long x2 asm("x2") = a1;
	register long x3 asm("x3") = a2;
	asm volatile("svc #0" : "=r"(ret), "=r"(err) : "r"(x0), "r"(x1), "r"(x2), "r"(x3));
	return (CrescentPosixSyscallResult) {ret, err};
}

static inline CrescentPosixSyscallResult __posix_syscall4(long num, long a0, long a1, long a2, long a3) {
	register long ret asm("x0");
	register long err asm("x1");
	register long x0 asm("x0") = num;
	register long x1 asm("x1") = a0;
	register long x2 asm("x2") = a1;
	register long x3 asm("x3") = a2;
	register long x4 asm("x4") = a3;
	asm volatile("svc #0" : "=r"(ret), "=r"(err) : "r"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4));
	return (CrescentPosixSyscallResult) {ret, err};
}

static inline CrescentPosixSyscallResult __posix_syscall5(long num, long a0, long a1, long a2, long a3, long a4) {
	register long ret asm("x0");
	register long err asm("x1");
	register long x0 asm("x0") = num;
	register long x1 asm("x1") = a0;
	register long x2 asm("x2") = a1;
	register long x3 asm("x3") = a2;
	register long x4 asm("x4") = a3;
	register long x5 asm("x5") = a4;
	asm volatile("svc #0" : "=r"(ret), "=r"(err)
		: "r"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5));
	return (CrescentPosixSyscallResult) {ret, err};
}

static inline CrescentPosixSyscallResult __posix_syscall6(long num, long a0, long a1, long a2, long a3, long a4, long a5) {
	register long ret asm("x0");
	register long err asm("x1");
	register long x0 asm("x0") = num;
	register long x1 asm("x1") = a0;
	register long x2 asm("x2") = a1;
	register long x3 asm("x3") = a2;
	register long x4 asm("x4") = a3;
	register long x5 asm("x5") = a4;
	register long x6 asm("x6") = a5;
	asm volatile("svc #0" : "=r"(ret), "=r"(err)
		: "r"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x6));
	return (CrescentPosixSyscallResult) {ret, err};
}

#endif
