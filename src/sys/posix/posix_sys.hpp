#pragma once
#include "types.hpp"
#include "errno.hpp"
#include "arch/arch_syscalls.hpp"

using pid_t = i32;

using off64_t = i64;
using time64_t = i64;

struct timespec64 {
	time64_t tv_sec;
	long tv_nsec;
};

struct sigset_t {
	unsigned long value;
};
