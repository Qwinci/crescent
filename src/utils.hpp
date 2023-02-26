#pragma once

template<typename T, typename V>
inline T cast(V value) {
	return reinterpret_cast<T>(value);
}

template<typename T, typename V>
constexpr T as(V value) {
	return static_cast<T>(value);
}

template<typename T>
inline T offset(T value, isize offset) {
	return reinterpret_cast<T>(reinterpret_cast<usize>(value) + offset);
}
