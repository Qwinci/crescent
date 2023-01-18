#include "types.hpp"

namespace noalloc {
	constexpr usize strlen(const char* str) {
		usize len = 0;
		for (; *str; ++str) ++len;
		return len;
	}

	class String {
	public:
		constexpr String(const char* str) : str {str}, l {strlen(str)} {} // NOLINT(google-explicit-constructor)

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
	private:
		const char* str;
		usize l;
	};
}