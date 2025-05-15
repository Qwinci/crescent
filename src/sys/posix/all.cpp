#include "arch/arch_syscalls.hpp"
#include "array.hpp"
#include "crescent/posix_syscalls.h"
#include "crescent/syscalls.h"
#include "errno.hpp"

void posix_mmap(SyscallFrame* frame);
void posix_munmap(SyscallFrame* frame);
void posix_mprotect(SyscallFrame* frame);

void posix_sigprocmask(SyscallFrame* frame);
void posix_sigaction(SyscallFrame* frame);
void posix_sigrestore(SyscallFrame* frame);
void posix_tgkill(SyscallFrame* frame);
void posix_kill(SyscallFrame* frame);

using Fn = void (*)(SyscallFrame* frame);

#define E(num) ((num) - static_cast<CrescentPosixSyscall>(SYS_POSIX_START))

static constexpr auto generate_syscall_array() {
	kstd::array<Fn, 256> arr {};
	arr[E(SYS_POSIX_MMAP)] = posix_mmap;
	arr[E(SYS_POSIX_MUNMAP)] = posix_munmap;
	arr[E(SYS_POSIX_MPROTECT)] = posix_mprotect;
	arr[E(SYS_POSIX_SIGPROCMASK)] = posix_sigprocmask;
	arr[E(SYS_POSIX_SIGACTION)] = posix_sigaction;
	arr[E(SYS_POSIX_SIGRESTORE)] = posix_sigrestore;
	arr[E(SYS_POSIX_TGKILL)] = posix_tgkill;
	arr[E(SYS_POSIX_KILL)] = posix_kill;

	return arr;
}

#undef E

constexpr auto POSIX_SYSCALL_ARRAY = generate_syscall_array();

void handle_posix_syscall(usize num, SyscallFrame* frame) {
	if (num - SYS_POSIX_START > 256 || !POSIX_SYSCALL_ARRAY[num - SYS_POSIX_START]) {
		*frame->error() = ENOSYS;
		return;
	}
	POSIX_SYSCALL_ARRAY[num - SYS_POSIX_START](frame);
}
