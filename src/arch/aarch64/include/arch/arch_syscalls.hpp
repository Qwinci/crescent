#pragma once
#include "types.hpp"
#include "arch/aarch64/interrupts/exceptions.hpp"

struct SyscallFrame : ExceptionFrame {
	constexpr u64* num() {
		return &x[0];
	}

	constexpr u64* ret() {
		return &x[0];
	}

	constexpr u64* error() {
		return &x[1];
	}

	constexpr u64* arg0() {
		return &x[1];
	}

	constexpr u64* arg1() {
		return &x[2];
	}

	constexpr u64* arg2() {
		return &x[3];
	}

	constexpr u64* arg3() {
		return &x[4];
	}

	constexpr u64* arg4() {
		return &x[5];
	}

	constexpr u64* arg5() {
		return &x[6];
	}
};
