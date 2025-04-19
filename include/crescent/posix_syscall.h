#ifndef CRESCENT_POSIX_SYSCALL_H
#define CRESCENT_POSIX_SYSCALL_H

#ifdef __x86_64__
#include "x86/posix_arch_syscall.h"
#elif defined(__aarch64__)
#include "aarch64/posix_arch_syscall.h"
#endif

#ifdef __cplusplus

inline CrescentPosixSyscallResult posix_syscall(long num) {
	return __posix_syscall0(num);
}

template<typename A0>
inline CrescentPosixSyscallResult posix_syscall(long num, A0 a0) {
	return __posix_syscall1(num, (long) a0);
}

template<typename A0, typename A1>
inline CrescentPosixSyscallResult posix_syscall(long num, A0 a0, A1 a1) {
	return __posix_syscall2(num, (long) a0, (long) a1);
}

template<typename A0, typename A1, typename A2>
inline CrescentPosixSyscallResult posix_syscall(long num, A0 a0, A1 a1, A2 a2) {
	return __posix_syscall3(num, (long) a0, (long) a1, (long) a2);
}

template<typename A0, typename A1, typename A2, typename A3>
inline CrescentPosixSyscallResult posix_syscall(long num, A0 a0, A1 a1, A2 a2, A3 a3) {
	return __posix_syscall4(num, (long) a0, (long) a1, (long) a2, (long) a3);
}

template<typename A0, typename A1, typename A2, typename A3, typename A4>
inline CrescentPosixSyscallResult posix_syscall(long num, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4) {
	return __posix_syscall5(num, (long) a0, (long) a1, (long) a2, (long) a3, (long) a4);
}

template<typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
inline CrescentPosixSyscallResult posix_syscall(long num, A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5) {
	return __posix_syscall6(num, (long) a0, (long) a1, (long) a2, (long) a3, (long) a4, (long) a5);
}

#else

#define __posix_syscall_NUM_ARGS_HELPER(a0, a1, a2, a3, a4, a5, a6, num, ...) num
#define __posix_syscall_NUM_ARGS(...) __posix_syscall_NUM_ARGS_HELPER(__VA_ARGS__, 6, 5, 4, 3, 2, 1, 0)
#define __posix_syscall_CONCAT_HELPER(a, b) a ## b
#define __posix_syscall_CONCAT(a, b) __posix_syscall_CONCAT_HELPER(a, b)

#define __posix_syscall_HELPER0(num) __posix_syscall0((num))
#define __posix_syscall_HELPER1(num, a0) __posix_syscall1((num), (long) (a0))
#define __posix_syscall_HELPER2(num, a0, a1) __posix_syscall2((num), (long) (a0), (long) (a1))
#define __posix_syscall_HELPER3(num, a0, a1, a2) __posix_syscall3((num), (long) (a0), (long) (a1), (long) (a2))
#define __posix_syscall_HELPER4(num, a0, a1, a2, a3) __posix_syscall4((num), (long) (a0), (long) (a1), (long) (a2), (long) (a3))
#define __posix_syscall_HELPER5(num, a0, a1, a2, a3, a4) __posix_syscall5((num), (long) (a0), (long) (a1), (long) (a2), \
	(long) (a3), (long) (a4))
#define __posix_syscall_HELPER6(num, a0, a1, a2, a3, a4, a5) __posix_syscall6((num), (long) (a0), (long) (a1), (long) (a2), \
	(long) (a3), (long) (a4), (long) (a5))

#define posix_syscall(...) __posix_syscall_CONCAT(__posix_syscall_HELPER, __posix_syscall_NUM_ARGS(__VA_ARGS__))(__VA_ARGS__)

#endif

#endif
