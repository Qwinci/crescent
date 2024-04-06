#pragma once
#include "types.hpp"
#include "arch/aarch64/interrupts/exceptions.hpp"

struct SyscallFrame : ExceptionFrame {
	constexpr u64* num() {
		return &x0;
	}

	constexpr u64* ret() {
		return &x0;
	}

	constexpr u64* arg0() {
		return &x1;
	}

	constexpr u64* arg1() {
		return &x2;
	}

	constexpr u64* arg2() {
		return &x3;
	}

	constexpr u64* arg3() {
		return &x4;
	}

	constexpr u64* arg4() {
		return &x5;
	}

	constexpr u64* arg5() {
		return &x6;
	}
};
