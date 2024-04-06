#pragma once
#include "types.hpp"

struct SyscallFrame {
	u64 number;
	u64 a0;

	constexpr u64* num() {
		return &number;
	}

	constexpr u64* ret() {
		return &number;
	}

	constexpr u64* arg0() {
		return &a0;
	}
};
