#pragma once
#include "algorithm.hpp"
#include "cstddef.hpp"
#include "type_traits.hpp"

#ifdef TESTING
#include "types.hpp"
#include <cstdlib>
#include <cassert>
#include <cstring>

class Allocator {
public:
	void* alloc(usize size) {
		void* ptr = malloc(size + 8);
		*static_cast<u64*>(ptr) = size;

		return offset(ptr, void*, 8);
	}

	void free(void* ptr, usize size) {
		if (!ptr) {
			return;
		}

		ptr = offset(ptr, void*, -8);
		auto real_size = *static_cast<u64*>(ptr);
		assert(size == real_size);
		::free(ptr);
	}
};
Allocator ALLOCATOR {};

#else
#include "mem/malloc.hpp"
#include "new.hpp"
#include "cstring.hpp"
#endif

#include "optional.hpp"

namespace kstd {
	template<typename T>
	class vector {
	private:
		static constexpr bool is_copyable = is_trivially_copyable_v<T>;

	public:
		constexpr vector() = default;
		constexpr vector(vector&& other) {
			_data = other._data;
			_size = other._size;
			_cap = other._cap;
			other._data = nullptr;
			other._size = 0;
			other._cap = 0;
		}
		vector(const vector& other) {
			if (other._size) {
				_data = static_cast<T*>(ALLOCATOR.alloc(other._size * sizeof(T)));
				assert(_data);
				_size = other._size;
				_cap = other._size;

				if constexpr (is_copyable) {
					memcpy(_data, other._data, _size * sizeof(T));
				}
				else {
					for (size_t i = 0; i < _size; ++i) {
						new (&_data[i]) T {other._data[i]};
					}
				}
			}
		}

		constexpr vector& operator=(vector&& other) {
			_data = other._data;
			_size = other._size;
			_cap = other._cap;
			other._data = nullptr;
			other._size = 0;
			other._cap = 0;
			return *this;
		}

		vector& operator=(const vector& other) {
			if (&other == this) {
				return *this;
			}

			if (_cap) {
				for (size_t i = 0; i < _size; ++i) {
					_data[i].~T();
				}
				ALLOCATOR.free(_data, _cap * sizeof(T));
			}

			_data = static_cast<T*>(ALLOCATOR.alloc(other._size * sizeof(T)));
			assert(_data);
			_size = other._size;
			_cap = other._size;

			if constexpr (is_copyable) {
				memcpy(_data, other._data, _size * sizeof(T));
			}
			else {
				for (size_t i = 0; i < _size; ++i) {
					new (&_data[i]) T {other._data[i]};
				}
			}
			return *this;
		}

		constexpr T& operator[](size_t index) {
			assert(index < _size);
			return _data[index];
		}

		constexpr const T& operator[](size_t index) const {
			assert(index < _size);
			return _data[index];
		}

		constexpr T* data() {
			return _data;
		}

		constexpr const T* data() const {
			return _data;
		}

		[[nodiscard]] constexpr size_t size() const {
			return _size;
		}

		[[nodiscard]] constexpr size_t capacity() const {
			return _cap;
		}

		[[nodiscard]] constexpr bool is_empty() const {
			return !_size;
		}

		void push(T&& value) {
			reserve(1);
			new (&_data[_size++]) T {std::move(value)};
		}

		void push(const T& value) {
			reserve(1);
			new (&_data[_size++]) T {value};
		}

		optional<T> pop() {
			if (is_empty()) {
				return {};
			}
			return std::move(_data[--_size]);
		}

		optional<T> remove(usize index) {
			if (index >= _size) {
				return {};
			}
			auto elem = std::move(_data[index]);
			for (usize i = index + 1; i < _size; ++i) {
				_data[i - 1] = std::move(_data[i]);
			}
			_size -= 1;
			return elem;
		}

		void reserve(size_t amount) {
			if (_size + amount > _cap) {
				auto new_cap = max(_cap < 8 ? 8 : (_cap + _cap / 2), _cap + amount);
				auto* new_data = static_cast<T*>(ALLOCATOR.alloc(new_cap * sizeof(T)));
				assert(new_data);

				if constexpr (is_copyable) {
					memcpy(new_data, _data, _size * sizeof(T));
				}
				else {
					for (usize i = 0; i < _size; ++i) {
						new (&new_data[i]) T {std::move(_data[i])};
					}
				}

				for (usize i = 0; i < _size; ++i) {
					_data[i].~T();
				}

				ALLOCATOR.free(_data, _cap * sizeof(T));
				_data = new_data;
				_cap = new_cap;
			}
		}

		void resize(size_t new_size) {
			if (new_size <= _size) {
				for (usize i = new_size; i < _size; ++i) {
					_data[i].~T();
				}
				_size = new_size;
			}
			else {
				auto amount = new_size - _size;
				reserve(amount);
				for (usize i = 0; i < amount; ++i) {
					new (&_data[_size++]) T {};
				}
			}
		}

		constexpr T* begin() {
			return _data;
		}

		constexpr const T* begin() const {
			return _data;
		}

		constexpr T* end() {
			return _data + _size;
		}

		constexpr const T* end() const {
			return _data + _size;
		}

		constexpr T* front() {
			return _data;
		}

		constexpr T* back() {
			return _data;
		}

		void clear() {
			for (size_t i = 0; i < _size; ++i) {
				_data[i].~T();
			}
			_size = 0;
		}

		void shrink_to_fit() {
			if (!_size && _cap) {
				ALLOCATOR.free(_data, _cap * sizeof(T));
				_data = nullptr;
				_cap = 0;
			}
		}

		~vector() {
			for (size_t i = 0; i < _size; ++i) {
				_data[i].~T();
			}
			ALLOCATOR.free(_data, _cap * sizeof(T));
		}

	private:
		T* _data {};
		size_t _size {};
		size_t _cap {};
	};
}
