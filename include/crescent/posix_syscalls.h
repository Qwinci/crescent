#ifndef CRESCENT_POSIX_SYSCALLS_H
#define CRESCENT_POSIX_SYSCALLS_H

typedef enum CrescentPosixSyscall {
	SYS_POSIX_MMAP = 0x1000,
	SYS_POSIX_MUNMAP,
	SYS_POSIX_MPROTECT
} CrescentPosixSyscall;

#endif
