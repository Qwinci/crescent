#pragma once
#include "cstddef.hpp"
#include "initializer_list.hpp"

namespace kstd {
	template<typename T, size_t N>
	struct array {
		constexpr T* begin() {
			return data;
		}
		constexpr const T* begin() const {
			return data;
		}

		constexpr T* end() {
			return data;
		}
		constexpr const T* end() const {
			return data;
		}

		[[nodiscard]] constexpr size_t size() const {
			return N;
		}

		constexpr T& operator[](size_t index) {
			return data[index];
		}
		constexpr const T& operator[](size_t index) const {
			return data[index];
		}

		T data[N];
	};
}
