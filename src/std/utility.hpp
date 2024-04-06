#pragma once
#include "type_traits.hpp"

namespace kstd {
	[[noreturn]] inline void unreachable() {
		__builtin_unreachable();
	}

	template<typename T1, typename T2>
	struct pair {
		T1 first;
		T2 second;
	};
}

namespace std {
	template<typename T>
	constexpr kstd::remove_reference_t<T>&& move(T&& value) {
		return static_cast<kstd::remove_reference_t<T>&&>(value);
	}

	template<typename T>
	constexpr T&& forward(kstd::remove_reference_t<T>& value) {
		return static_cast<T&&>(value);
	}

	template<typename T>
	constexpr T&& forward(kstd::remove_reference_t<T>&& value) {
		return static_cast<T&&>(value);
	}
}
