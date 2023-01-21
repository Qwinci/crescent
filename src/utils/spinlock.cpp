#include "spinlock.hpp"

void Spinlock::lock() {
	while (true) {
		if (!atomic_exchange_explicit(&inner, true, memory_order_acquire)) {
			break;
		}

		while (atomic_load_explicit(&inner, memory_order_relaxed)) {
			__builtin_ia32_pause();
		}
	}
}

bool Spinlock::try_lock() {
	return !atomic_exchange_explicit(&inner, true, memory_order_acquire);
}

void Spinlock::unlock() {
	atomic_store_explicit(&inner, false, memory_order_release);
}
