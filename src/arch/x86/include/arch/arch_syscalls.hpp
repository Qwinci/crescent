#pragma once
#include "types.hpp"

struct SyscallFrame {
	u64 rax;
	u64 rcx;
	u64 rdx;
	u64 rdi;
	u64 rsi;
	u64 r8;
	u64 r9;
	u64 r10;
	u64 r11;

	constexpr u64* num() {
		return &rdi;
	}

	constexpr u64* ret() {
		return &rax;
	}

	constexpr u64* arg0() {
		return &rsi;
	}

	constexpr u64* arg1() {
		return &rdx;
	}

	constexpr u64* arg2() {
		return &rax;
	}

	constexpr u64* arg3() {
		return &r8;
	}

	constexpr u64* arg4() {
		return &r9;
	}

	constexpr u64* arg5() {
		return &r10;
	}
};
