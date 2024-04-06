#pragma once
#include "utility.hpp"
#include "new.hpp"
#include "assert.hpp"

template<typename T>
class ManuallyInit {
public:
	void initialize(T&& value) {
		new (storage.data) T {std::move(value)};
	}

	template<typename... Args>
	void initialize(Args... args) {
		new (storage.data) T {std::forward<Args>(args)...};
	}

	void destroy() {
		reinterpret_cast<T*>(storage.data)->~T();
	}

	T* operator->() {
		return reinterpret_cast<T*>(storage.data);
	}

	T& operator*() {
		return *reinterpret_cast<T*>(storage.data);
	}

private:
	struct alignas(T) Storage {
		char data[sizeof(T)];
	};

	Storage storage;
};
