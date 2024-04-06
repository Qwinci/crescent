#pragma once
#include "types.hpp"

struct Process;

struct ArchThread {
	constexpr ArchThread() = default;
	ArchThread(void (*fn)(void*), void* arg, Process* process);
	~ArchThread();

	u8* sp {};
	u8* syscall_sp {};
	u8* saved_user_sp {};
	usize handler_ip {};
	usize handler_sp {};
	usize* kernel_stack_base {};
	u8* user_stack_base {};
	u8* simd {};
};

static_assert(offsetof(ArchThread, sp) == 0);
static_assert(offsetof(ArchThread, syscall_sp) == 8);
static_assert(offsetof(ArchThread, saved_user_sp) == 16);
static_assert(offsetof(ArchThread, handler_ip) == 24);
static_assert(offsetof(ArchThread, handler_sp) == 32);
