#pragma once
#include "types.hpp"

struct Process;

struct ArchThread {
	constexpr ArchThread() : self {this} {}
	ArchThread(void (*fn)(void*), void* arg, Process* process);
	~ArchThread();

	ArchThread* self;
	u8* sp {};
	u8* syscall_sp {};
	u8* saved_user_sp {};
	usize handler_ip {};
	usize handler_sp {};
	usize* kernel_stack_base {};
	u8* user_stack_base {};
	u8* simd {};
	usize fs_base {};
	usize gs_base {};
};

static_assert(offsetof(ArchThread, sp) == 8);
static_assert(offsetof(ArchThread, syscall_sp) == 16);
static_assert(offsetof(ArchThread, saved_user_sp) == 24);
static_assert(offsetof(ArchThread, handler_ip) == 32);
static_assert(offsetof(ArchThread, handler_sp) == 40);
