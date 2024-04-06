#pragma once
#include "cstddef.hpp"

namespace std {
	template<typename T>
	class initializer_list {
	public:
		using value_type = T;
		using reference = const T&;
		using const_reference = const T&;
		using size_type = size_t;

		using iterator = const T*;
		using const_iterator = const T*;

		constexpr initializer_list() noexcept = default;

		[[nodiscard]] constexpr size_t size() const noexcept {
			return _len;
		}

		constexpr const T* begin() const noexcept {
			return _start;
		}

		constexpr const T* end() const noexcept {
			return _start + _len;
		}

	private:
		const T* _start {};
		size_t _len {};
	};
}
