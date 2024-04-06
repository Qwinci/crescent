#pragma once
#include "bit.hpp"

namespace kstd {
	template<typename T>
	constexpr int log2(T value) {
		if (value == 0) {
			return 0;
		}
		return bit_width(value) - 1;
	}

	template<typename T>
	constexpr T pow2(unsigned int power) {
		return T {1} << power;
	}
}
