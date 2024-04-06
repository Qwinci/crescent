#ifndef CRESCENT_SYSCALL_H
#define CRESCENT_SYSCALL_H

#ifdef __x86_64__
#include "x86/arch_syscall.h"
#elif defined(__aarch64__)
#include "aarch64/arch_syscall.h"
#endif

#ifdef __cplusplus

inline long syscall(long num) {
	return __syscall0(num);
}

template<typename A0>
inline long syscall(long num, A0 a0) {
	return __syscall1(num, (long) a0);
}

template<typename A0, typename A1>
inline long syscall(long num, A0 a0, A1 a1) {
	return __syscall2(num, (long) a0, (long) a1);
}

template<typename A0, typename A1, typename A2>
inline long syscall(long num, A0 a0, A1 a1, A2 a2) {
	return __syscall3(num, (long) a0, (long) a1, (long) a2);
}

template<typename A0, typename A1, typename A2, typename A3>
inline long syscall(long num, A0 a0, A1 a1, A2 a2, A3 a3) {
	return __syscall4(num, (long) a0, (long) a1, (long) a2, (long) a3);
}

template<typename A0, typename A1, typename A2, typename A3, typename A4>
inline long syscall(long num, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4) {
	return __syscall5(num, (long) a0, (long) a1, (long) a2, (long) a3, (long) a4);
}

template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
inline long syscall(long num, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5) {
	return __syscall6(num, (long) a0, (long) a1, (long) a2, (long) a3, (long) a4, (long) a5);
}

#else

#define __SYSCALL_NUM_ARGS_HELPER(a0, a1, a2, a3, a4, a5, a6, num, ...) num
#define __SYSCALL_NUM_ARGS(...) __SYSCALL_NUM_ARGS_HELPER(__VA_ARGS__, 6, 5, 4, 3, 2, 1, 0)
#define __SYSCALL_CONCAT_HELPER(a, b) a ## b
#define __SYSCALL_CONCAT(a, b) __SYSCALL_CONCAT_HELPER(a, b)

#define __SYSCALL_HELPER0(num) __syscall0((num))
#define __SYSCALL_HELPER1(num, a0) __syscall1((num), (long) (a0))
#define __SYSCALL_HELPER2(num, a0, a1) __syscall2((num), (long) (a0), (long) (a1))
#define __SYSCALL_HELPER3(num, a0, a1, a2) __syscall3((num), (long) (a0), (long) (a1), (long) (a2))
#define __SYSCALL_HELPER4(num, a0, a1, a2, a3) __syscall4((num), (long) (a0), (long) (a1), (long) (a2), (long) (a3))
#define __SYSCALL_HELPER5(num, a0, a1, a2, a3, a4) __syscall5((num), (long) (a0), (long) (a1), (long) (a2), \
	(long) (a3), (long) (a4))
#define __SYSCALL_HELPER6(num, a0, a1, a2, a3, a4, a5) __syscall6((num), (long) (a0), (long) (a1), (long) (a2), \
	(long) (a3), (long) (a4), (long) (a5))

#define syscall(...) __SYSCALL_CONCAT(__SYSCALL_HELPER, __SYSCALL_NUM_ARGS(__VA_ARGS__))(__VA_ARGS__)

#endif

#endif
