#pragma once
#include "utility.hpp"
#include "new.hpp"
#include "assert.hpp"

template<typename T>
class ManuallyInit {
public:
	void initialize(T&& value) {
		new (storage.data) T {std::move(value)};
		initialized = true;
	}

	template<typename... Args>
	void initialize(Args... args) {
		new (storage.data) T {std::forward<Args>(args)...};
		initialized = true;
	}

	void destroy() {
		kstd::launder(reinterpret_cast<T*>(storage.data))->~T();
	}

	T* operator->() {
		return kstd::launder(reinterpret_cast<T*>(storage.data));
	}

	T& operator*() {
		return *kstd::launder(reinterpret_cast<T*>(storage.data));
	}

	[[nodiscard]] constexpr bool is_initialized() const {
		return initialized;
	}

private:
	struct alignas(T) Storage {
		char data[sizeof(T)];
	};

	Storage storage;
	bool initialized;
};
