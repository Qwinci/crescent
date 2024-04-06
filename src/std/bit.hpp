#pragma once
#include "concepts.hpp"
#include "limits.hpp"

namespace kstd {
	template<integral T>
	constexpr T byteswap(T value) noexcept {
		if constexpr (sizeof(T) == 1) {
			return value;
		}
		else if constexpr (sizeof(T) == 2) {
			return __builtin_bswap16(value);
		}
		else if constexpr (sizeof(T) == 4) {
			return __builtin_bswap32(value);
		}
		else if constexpr (sizeof(T) == 8) {
			return __builtin_bswap64(value);
		}
		else {
			__builtin_unreachable();
		}
	}

	template<typename T>
	constexpr bool has_single_bit(T value) {
		return value && !(value & (value - 1));
	}

	template<typename T>
	constexpr int countl_zero(T value) {
		constexpr auto digits = numeric_limits<T>::digits;
		if (value == 0) {
			return digits;
		}

		constexpr auto ull_digits = numeric_limits<unsigned long long>::digits;
		constexpr auto ul_digits = numeric_limits<unsigned long>::digits;
		constexpr auto u_digits = numeric_limits<unsigned int>::digits;

		if constexpr (digits <= u_digits) {
			return __builtin_clz(value) - (u_digits - digits);
		}
		else if constexpr (digits <= ul_digits) {
			return __builtin_clzl(value) - (ul_digits - digits);
		}
		else if constexpr (digits <= ull_digits) {
			return __builtin_clzll(value) - (ull_digits - digits);
		}
		else {
			__builtin_unreachable();
		}
	}

	template<typename T>
	constexpr int countl_one(T value) {
		return countl_zero(~value);
	}

	template<typename T>
	constexpr int countr_zero(T value) {
		constexpr auto digits = numeric_limits<T>::digits;
		if (value == 0) {
			return digits;
		}

		constexpr auto ull_digits = numeric_limits<unsigned long long>::digits;
		constexpr auto ul_digits = numeric_limits<unsigned long>::digits;
		constexpr auto u_digits = numeric_limits<unsigned int>::digits;

		if constexpr (digits <= u_digits) {
			return __builtin_ctz(value);
		}
		else if constexpr (digits <= ul_digits) {
			return __builtin_ctzl(value);
		}
		else if constexpr (digits <= ull_digits) {
			return __builtin_ctzll(value);
		} else {
			__builtin_unreachable();
		}
	}

	template<typename T>
	constexpr int countr_one(T value) {
		return countr_zero(~value);
	}

	template<typename T>
	constexpr int bit_width(T value) {
		return numeric_limits<T>::digits - countl_zero(value);
	}

	template<typename T>
	constexpr T bit_ceil(T value) {
		return T(1) << bit_width(T(value - 1));
	}

	template<typename T>
	constexpr T bit_floor(T value) {
		if (value == 0) {
			return 0;
		}
		return T(1) << (bit_width(value) - 1);
	}

	template<typename T>
	constexpr int popcount(T value) {
		constexpr auto digits = numeric_limits<T>::digits;

		constexpr auto ull_digits = numeric_limits<unsigned long long>::digits;
		constexpr auto ul_digits = numeric_limits<unsigned long>::digits;
		constexpr auto u_digits = numeric_limits<unsigned int>::digits;

		if constexpr (digits <= u_digits) {
			return __builtin_popcount(value);
		}
		else if constexpr (digits <= ul_digits) {
			return __builtin_popcountl(value);
		}
		else if constexpr (digits <= ull_digits) {
			return __builtin_popcountll(value);
		}
		else {
			__builtin_unreachable();
		}
	}

	template<typename To, typename From>
	constexpr To bit_cast(const From& from) {
		return __builtin_bit_cast(To, from);
	}
}
