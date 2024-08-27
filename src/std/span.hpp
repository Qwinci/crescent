#pragma once
#include "cstddef.hpp"
#include "initializer_list.hpp"

namespace kstd {
	inline constexpr size_t dynamic_extent = size_t(-1);

	template<typename T>
	class span {
	public:
		constexpr span() = default;
		template<typename It>
		constexpr span(It first, size_t size)
			: _begin {first}, _size {size} {}

		template<typename It>
		constexpr span(It first, It last)
			: _begin {first}, _size {last - first} {}

		constexpr span(std::initializer_list<T> list)
			: _begin {list.begin()}, _size {list.size()} {}

		template<size_t N>
		constexpr span(const T (&arr)[N]) : _begin {&arr[0]}, _size {N} {}

		[[nodiscard]] constexpr const T* begin() const {
			return _begin;
		}

		[[nodiscard]] constexpr const T* end() const {
			return _begin + _size;
		}

		[[nodiscard]] constexpr size_t size() const {
			return _size;
		}

		[[nodiscard]] constexpr bool is_empty() const {
			return _size;
		}

		[[nodiscard]] constexpr const T* data() const {
			return _begin;
		}

		constexpr const T& operator[](size_t index) const {
			return _begin[index];
		}

	private:
		const T* _begin;
		size_t _size {};
	};
}
