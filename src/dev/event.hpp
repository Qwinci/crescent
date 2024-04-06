#pragma once
#include "sched/thread.hpp"
#include "functional.hpp"
#include "vector.hpp"

struct Event {
	void reset();
	void wait();
	bool wait_with_timeout(u64 max_us);

	void signal_one();
	void signal_all();

private:
	Spinlock<DoubleList<Thread, &Thread::misc_hook>> waiters {};
	volatile bool signaled {};
};

struct CallbackProducer {
	[[nodiscard]] usize add_callback(kstd::small_function<void()> callback);
	void remove_callback(usize index);

	void signal_one();
	void signal_all();

private:
	Spinlock<kstd::vector<kstd::small_function<void()>>> callbacks {};
};
