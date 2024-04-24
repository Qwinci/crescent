#pragma once
#include "dev/event.hpp"
#include "atomic.hpp"
#include "utils/irq_guard.hpp"

template<typename T>
struct Mutex;

template<typename T>
struct Mutex {
	constexpr Mutex() = default;
	constexpr explicit Mutex(T&& value) : value {std::move(value)} {}

	struct Guard {
		~Guard() {
			owner.inner.store(false, kstd::memory_order::release);
			owner.unlock_event.signal_one();
		}

		T* operator->() {
			return &owner.value;
		}

		T& operator*() {
			return owner.value;
		}

		Mutex& owner;
	};

	Guard lock() {
		IrqGuard irq_guard {};
		while (true) {
			if (!inner.exchange(true, kstd::memory_order::acquire)) {
				break;
			}
			unlock_event.wait();
		}
		return {*this};
	}

private:
	friend Guard;

	T value;
	Event unlock_event {};
	kstd::atomic<bool> inner {};
};

template<>
struct Mutex<void> {
	constexpr Mutex() = default;

	struct Guard {
		~Guard() {
			owner.inner.store(false, kstd::memory_order::release);
			owner.unlock_event.signal_one();
		}

		Mutex& owner;
	};

	Guard lock() {
		IrqGuard irq_guard {};
		while (true) {
			if (!inner.exchange(true, kstd::memory_order::acquire)) {
				break;
			}
			unlock_event.wait();
		}
		return {*this};
	}

private:
	friend Guard;

	Event unlock_event {};
	kstd::atomic<bool> inner {};
};
