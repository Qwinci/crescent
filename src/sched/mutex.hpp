#pragma once
#include <stdatomic.h>

struct Task;

struct Mutex {
	bool try_lock();
	void lock();
	void unlock();
private:
	atomic_bool inner;
	Task* waiting_tasks {};
};