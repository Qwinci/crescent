#pragma once

template<typename T>
constexpr T&& move(T&& value) {
	return static_cast<T&&>(value);
}

template<typename T, typename D>
constexpr T* dyn_cast(D* derived) {
	return static_cast<T*>(derived);
}
template<typename T, typename D>
constexpr T& dyn_cast(D& derived) {
	return static_cast<T&>(derived);
}