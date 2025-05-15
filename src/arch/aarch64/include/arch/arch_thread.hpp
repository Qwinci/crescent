#pragma once
#include "types.hpp"
#include "sched/sysv.hpp"

struct Process;

struct ArchThread {
	constexpr ArchThread() = default;
	ArchThread(void (*fn)(void*), void* arg, Process* process);
	ArchThread(const SysvInfo& sysv, Process* process);
	~ArchThread();

	u8* sp {};
	u8* syscall_sp {};
	usize handler_ip {};
	usize handler_sp {};
	usize* kernel_stack_base {};
	u8* user_stack_base {};
	u8* simd {};
	usize tpidr_el0 {};
};

// used in arch_sched.cpp
static_assert(offsetof(ArchThread, sp) == 0);
// used in exceptions.S
static_assert(offsetof(ArchThread, syscall_sp) == 8);
// used in user.S
static_assert(offsetof(ArchThread, handler_ip) == 16);
static_assert(offsetof(ArchThread, handler_sp) == 24);
