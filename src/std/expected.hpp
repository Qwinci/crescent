#pragma once
#include "type_traits.hpp"

namespace kstd {
	template<typename T>
	struct unexpected {
		T value;
	};

	template<typename T, typename E>
	class expected {
	public:
		constexpr expected(T&& value) // NOLINT(*-explicit-constructor)
			: data {.value {std::move(value)}}, success {true} {}
		constexpr expected(const T& value)  // NOLINT(*-explicit-constructor)
			: data {.value {value}}, success {true} {}
		constexpr expected(E&& error) // NOLINT(*-explicit-constructor)
			: data {.error {std::move(error)}}, success {false} {}
		constexpr expected(const E& value)  // NOLINT(*-explicit-constructor)
			: data {.error {value}}, success {false} {}

		constexpr T& value() & {
			return data.value;
		}
		constexpr const T& value() const & {
			return data.value;
		}

		constexpr T&& value() && {
			return std::move(data.value);
		}

		constexpr E& error() & {
			return data.error;
		}
		constexpr const E& error() const & {
			return data.error;
		}
		constexpr E&& error() && {
			return std::move(data.error);
		}

		constexpr explicit operator bool() const {
			return success;
		}

		constexpr ~expected() {
			if (success) {
				data.value.~T();
			}
			else {
				data.error.~E();
			}
		}

	private:
		union Data {
			constexpr ~Data() {}
			T value;
			E error;
		} data;
		bool success;
	};
}
