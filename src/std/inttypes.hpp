#pragma once
#include "string_view.hpp"

namespace kstd {
	namespace __detail {
		constexpr char to_lower(char c) {
			if (c >= 'A' && c <= 'Z') {
				return static_cast<char>(c | 1 << 5);
			}
			else {
				return c;
			}
		}
	}

	template<typename T>
	constexpr T to_integer(kstd::string_view str, int base) {
		static constexpr char CHARS[] = "0123456789abcdef";

		T value {};
		for (auto c : str) {
			c = __detail::to_lower(c);
			if (c < '0' || c > CHARS[base - 1]) {
				return value;
			}
			value *= base;
			value += c <= '9' ? (c - '0') : (c - 'a' + 10);
		}

		return value;
	}
}
