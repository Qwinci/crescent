#pragma once

template<typename T>
constexpr const T& max(const T& a, const T& b) {
	return a > b ? a : b;
}

template<typename T>
constexpr const T& min(const T& a, const T& b) {
	return a < b ? a : b;
}

#define ALIGNUP(value, align) (((value) + ((align) - 1)) & ~((align) - 1))
#define ALIGNDOWN(value, align) ((value) & ~((align) - 1))
