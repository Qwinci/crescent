#pragma once
#include <stdatomic.h>

class Spinlock {
public:
	void lock();
	bool try_lock();
	void unlock();
private:
	atomic_bool inner = false;
};