#pragma once
#include "types.hpp"

template<typename T, usize N>
struct Array {
	[[nodiscard]] constexpr usize len() const {
		return N;
	}

	constexpr const T& operator[](usize i) const {
		return data[i];
	}

	constexpr T& operator[](usize i) {
		return data[i];
	}

	constexpr T* begin() {
		return data;
	}

	[[nodiscard]] constexpr const T* begin() const {
		return data;
	}

	constexpr T* end() {
		return data + N;
	}

	[[nodiscard]] constexpr const T* end() const {
		return data + N;
	}

	T data[N];
};