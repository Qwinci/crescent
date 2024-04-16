#pragma once
#include "utility.hpp"
#include "new.hpp"
#include "assert.hpp"

template<typename T>
class ManuallyDestroy {
public:
	ManuallyDestroy() {
		new (storage.data) T {};
	}

	ManuallyDestroy(T&& value) { // NOLINT(*-explicit-constructor)
		new (storage.data) T {std::move(value)};
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

private:
	struct alignas(T) Storage {
		char data[sizeof(T)];
	};

	Storage storage;
};
