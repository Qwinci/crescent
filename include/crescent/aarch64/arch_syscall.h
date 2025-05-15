#ifndef CRESCENT_ARCH_SYSCALL_H
#define CRESCENT_ARCH_SYSCALL_H

static inline long __syscall0(long num) {
	register long ret __asm__("x0");
	register long x0 __asm__("x0") = num;
	__asm__ volatile("svc #0" : "=r"(ret) : "r"(x0));
	return ret;
}

static inline long __syscall1(long num, long a0) {
	register long ret __asm__("x0");
	register long x0 __asm__("x0") = num;
	register long x1 __asm__("x1") = a0;
	__asm__ volatile("svc #0" : "=r"(ret) : "r"(x0), "r"(x1));
	return ret;
}

static inline long __syscall2(long num, long a0, long a1) {
	register long ret __asm__("x0");
	register long x0 __asm__("x0") = num;
	register long x1 __asm__("x1") = a0;
	register long x2 __asm__("x2") = a1;
	__asm__ volatile("svc #0" : "=r"(ret) : "r"(x0), "r"(x1), "r"(x2));
	return ret;
}

static inline long __syscall3(long num, long a0, long a1, long a2) {
	register long ret __asm__("x0");
	register long x0 __asm__("x0") = num;
	register long x1 __asm__("x1") = a0;
	register long x2 __asm__("x2") = a1;
	register long x3 __asm__("x3") = a2;
	__asm__ volatile("svc #0" : "=r"(ret) : "r"(x0), "r"(x1), "r"(x2), "r"(x3));
	return ret;
}

static inline long __syscall4(long num, long a0, long a1, long a2, long a3) {
	register long ret __asm__("x0");
	register long x0 __asm__("x0") = num;
	register long x1 __asm__("x1") = a0;
	register long x2 __asm__("x2") = a1;
	register long x3 __asm__("x3") = a2;
	register long x4 __asm__("x4") = a3;
	__asm__ volatile("svc #0" : "=r"(ret) : "r"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4));
	return ret;
}

static inline long __syscall5(long num, long a0, long a1, long a2, long a3, long a4) {
	register long ret __asm__("x0");
	register long x0 __asm__("x0") = num;
	register long x1 __asm__("x1") = a0;
	register long x2 __asm__("x2") = a1;
	register long x3 __asm__("x3") = a2;
	register long x4 __asm__("x4") = a3;
	register long x5 __asm__("x5") = a4;
	__asm__ volatile("svc #0" : "=r"(ret)
		: "r"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5));
	return ret;
}

static inline long __syscall6(long num, long a0, long a1, long a2, long a3, long a4, long a5) {
	register long ret __asm__("x0");
	register long x0 __asm__("x0") = num;
	register long x1 __asm__("x1") = a0;
	register long x2 __asm__("x2") = a1;
	register long x3 __asm__("x3") = a2;
	register long x4 __asm__("x4") = a3;
	register long x5 __asm__("x5") = a4;
	register long x6 __asm__("x6") = a5;
	__asm__ volatile("svc #0" : "=r"(ret)
		: "r"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x6));
	return ret;
}

#endif
