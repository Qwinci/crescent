#pragma once
#include "utility.hpp"

#ifdef TESTING
#include <cassert>
#else
#include "assert.hpp"
#include "new.hpp"
#endif

namespace kstd {
	template<typename T>
	class optional {
	public:
		using value_type = T;

		constexpr optional() noexcept : data {.size {}}, _has_value {false} {}
		constexpr optional(optional&& other) : data {.size {}}, _has_value {false} {
			if (other._has_value) {
				new (&data.obj) T {std::move(other.data.obj)};
				other._has_value = false;
				_has_value = true;
			}
		}
		constexpr optional(const optional& other) : data {.size {}}, _has_value {false} {
			if (other._has_value) {
				new (&data.obj) T {other.data.obj};
				_has_value = true;
			}
		}

		template<typename U = T>
		constexpr optional(U&& value) requires(!is_same_v<U, optional>) // NOLINT(*-explicit-constructor)
			: data {.obj {static_cast<U&&>(value)}}, _has_value {true} {}

		constexpr ~optional() {
			if (_has_value) {
				data.obj.~T();
			}
		}

		constexpr optional& operator=(optional&& other) {
			if (_has_value) {
				data.obj.~T();
				_has_value = false;
			}

			if (other._has_value) {
				new (&data.obj) T {std::move(other.data.obj)};
				other._has_value = false;
				_has_value = true;
			}
			return *this;
		}

		constexpr explicit operator bool() const noexcept {
			return _has_value;
		}
		[[nodiscard]] constexpr bool has_value() const noexcept {
			return _has_value;
		}

		constexpr T& value() & {
			return data.obj;
		}
		constexpr const T& value() const & {
			return data.obj;
		}

		constexpr T value_or(T&& other) {
			if (_has_value) {
				return data.obj;
			}
			else {
				return other;
			}
		}

		constexpr T&& value() && {
			assert(_has_value);
			return std::move(data.obj);
		}
		constexpr const T&& value() const && {
			return std::move(data.obj);
		}

		constexpr T* operator->() {
			assert(_has_value);
			return &data.obj;
		}

		constexpr const T* operator->() const {
			assert(_has_value);
			return &data.obj;
		}

		constexpr void reset() noexcept {
			if (_has_value) {
				data.obj.~T();
				_has_value = false;
			}
		}

	private:
		union Data {
			constexpr ~Data() {}

			char size[sizeof(T)];
			T obj;
		} data;
		bool _has_value;
	};
}
