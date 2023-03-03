#pragma once
#include "types.hpp"

namespace noalloc {
	constexpr usize strlen(const char* str) {
		usize len = 0;
		while (*str++) ++len;
		return len;
	}

	class String {
	public:
		constexpr String(const char* str) : str {str}, l {strlen(str)} {} // NOLINT(google-explicit-constructor)
		constexpr String(const char* str, usize len) : str {str}, l {len} {}

		[[nodiscard]] constexpr usize len() const {
			return l;
		}

		[[nodiscard]] constexpr const char* data() const {
			return str;
		}

		constexpr bool operator==(const String& other) const {
			if (l != other.l) return false;

			for (usize i = 0; i < l; ++i) {
				if (str[i] != other.str[i]) return false;
			}

			return true;
		}

		constexpr char operator[](usize index) const {
			return str[index];
		}
	private:
		const char* str;
		usize l;
	};
}