#pragma once
#include "cstddef.hpp"
#include "algorithm.hpp"

namespace kstd {
	template<typename T>
	class basic_string_view {
	public:
		using const_iterator = const T*;
		static constexpr size_t npos = static_cast<size_t>(-1);

		constexpr basic_string_view() noexcept = default;
		constexpr basic_string_view(const basic_string_view& other) = default;
		constexpr basic_string_view(basic_string_view&&) = default;
		constexpr basic_string_view(const T* s, size_t size) : _ptr {s}, _size {size} {}
		constexpr basic_string_view(const T* s) : _ptr {s}, _size {const_strlen(s)} {}
		constexpr basic_string_view(const T* first, const T* last) {
			_ptr = first;
			_size = last - first;
		}
		constexpr basic_string_view(kstd::nullptr_t) = delete;

		constexpr basic_string_view& operator=(const basic_string_view&) = default;
		constexpr basic_string_view& operator=(basic_string_view&&) = default;

		[[nodiscard]] constexpr const_iterator begin() const noexcept {
			return _ptr;
		}
		[[nodiscard]] constexpr const_iterator end() const noexcept {
			return _ptr + _size;
		}

		[[nodiscard]] constexpr const T& operator[](size_t index) const {
			return _ptr[index];
		}

		[[nodiscard]] constexpr const T& front() const {
			return _ptr[0];
		}
		[[nodiscard]] constexpr const T& back() const {
			return _ptr[_size - 1];
		}

		[[nodiscard]] constexpr const T* data() const {
			return _ptr;
		}

		[[nodiscard]] constexpr size_t size() const {
			return _size;
		}

		[[nodiscard]] constexpr bool is_empty() const {
			return _size == 0;
		}

		[[nodiscard]] constexpr bool operator==(basic_string_view other) const noexcept {
			if (_size != other._size) {
				return false;
			}
			for (size_t i = 0; i < _size; ++i) {
				if (_ptr[i] != other._ptr[i]) {
					return false;
				}
			}
			return true;
		}

		[[nodiscard]] constexpr bool starts_with(basic_string_view str) const noexcept {
			return basic_string_view {_ptr, min(_size, str._size)} == str;
		}
		[[nodiscard]] constexpr bool starts_with(T c) const noexcept {
			return !is_empty() && _ptr[0] == c;
		}

		[[nodiscard]] constexpr bool ends_with(basic_string_view str) const noexcept {
			return _size >= str._size && basic_string_view {_ptr + _size - str._size, str._size} == str;
		}
		[[nodiscard]] constexpr bool ends_with(T c) const noexcept {
			return !is_empty() && _ptr[_size - 1] == c;
		}

		constexpr void remove_prefix(size_t n) {
			_ptr += n;
			_size -= n;
		}
		constexpr bool remove_prefix(basic_string_view prefix) {
			if (starts_with(prefix)) {
				_ptr += prefix._size;
				_size -= prefix._size;
				return true;
			}
			else {
				return false;
			}
		}

		constexpr void remove_suffix(size_t n) {
			_size -= n;
		}
		constexpr bool remove_suffix(basic_string_view suffix) {
			if (ends_with(suffix)) {
				_size -= suffix._size;
				return true;
			}
			else {
				return false;
			}
		}

		[[nodiscard]] constexpr basic_string_view substr(size_t pos = 0, size_t count = npos) const {
			if (count == npos) {
				count = _size - pos;
			}
			else if (pos + count > _size) {
				count = 0;
			}
			return {_ptr + pos, count};
		}

		[[nodiscard]] constexpr size_t find(basic_string_view str, size_t pos = 0) const noexcept {
			if (_size < str._size) {
				return npos;
			}

			for (; pos <= _size - str._size; ++pos) {
				if (basic_string_view {_ptr + pos, str._size} == str) {
					return pos;
				}
			}
			return npos;
		}
		[[nodiscard]] constexpr size_t find(T c, size_t pos = 0) const noexcept {
			for (; pos < _size; ++pos) {
				if (_ptr[pos] == c) {
					return pos;
				}
			}
			return npos;
		}

		[[nodiscard]] constexpr size_t rfind(basic_string_view str, size_t pos = npos) const noexcept {
			if (_size < str._size) {
				return npos;
			}

			if (pos == npos) {
				pos = _size - str._size;
			}
			else if (pos > _size - str._size) {
				return npos;
			}

			while (true) {
				if (basic_string_view {_ptr + pos, str._size} == str) {
					return pos;
				}

				if (!pos--) {
					break;
				}
			}
			return npos;
		}
		[[nodiscard]] constexpr size_t rfind(T c, size_t pos = npos) const noexcept {
			if (is_empty()) {
				return npos;
			}

			if (pos == npos) {
				pos = _size - 1;
			}
			else if (pos > _size - 1) {
				return npos;
			}

			while (true) {
				if (_ptr[pos] == c) {
					return pos;
				}

				if (!pos--) {
					break;
				}
			}
			return npos;
		}

	private:
		static constexpr size_t const_strlen(const char* s) {
			size_t len = 0;
			while (*s++) ++len;
			return len;
		}

		const T* _ptr;
		size_t _size;
	};

	using string_view = basic_string_view<char>;
	using wstring_view = basic_string_view<wchar_t>;
}
