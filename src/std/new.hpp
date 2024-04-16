#pragma once
#include "cstddef.hpp"

constexpr void* operator new(size_t, void* ptr) {
	return ptr;
}

namespace kstd {
	template<typename T>
	[[nodiscard]] constexpr T* launder(T* ptr) {
		return __builtin_launder(ptr);
	}
}
