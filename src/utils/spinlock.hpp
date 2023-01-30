#pragma once
#include <stdatomic.h>

class Spinlock {
public:
	void lock();
	bool try_lock();
	void unlock();
	[[nodiscard]] inline bool get() const {
		return atomic_load_explicit(&inner, memory_order_acquire);
	}
private:
	atomic_bool inner = false;
};