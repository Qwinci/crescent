#pragma once

#define container_of(ptr, base, field) (cast<base*>(cast<u8*>(ptr) - offsetof(base, field)))

template<typename T>
struct remove_reference {
	using type = T;
};
template<typename T>
struct remove_reference<T&> {
	using type = T;
};
template<typename T>
struct remove_reference<T&&> {
	using type = T;
};

template<typename T>
typename remove_reference<T>::type&& move(T&& arg) {
	return static_cast<typename remove_reference<T>::type&&>(arg);
}