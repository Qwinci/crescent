#pragma once

template<typename T>
struct Optional {
	constexpr Optional() : m_has_value {false} {}
	constexpr explicit Optional(const T&& value) : m_value {value}, m_has_value {true} {}

	[[nodiscard]] constexpr bool has_value() const {
		return m_has_value;
	}

	constexpr T get() {
		if (!m_has_value) {
			__builtin_trap();
		}

		m_has_value = false;
		return move(m_value);
	}
private:
	union {
		char dummy[sizeof(T)] {};
		T m_value;
	};
	bool m_has_value;
};