#pragma once
#include "sched/thread.hpp"
#include "functional.hpp"
#include "vector.hpp"

struct Waitable {
	DoubleListHook hook {};
	Thread* thread {};
	bool in_list {};
};

struct Event {
	static usize wait_any(Event** events, usize count, u64 max_ns);

	void reset();
	void wait(bool consume = true);
	bool wait_with_timeout(u64 max_ns);

	void signal_one();
	void signal_one_if_not_pending();
	void signal_all();

	void signal_count(usize count);

	inline bool is_being_waited() {
		IrqGuard irq_guard {};
		auto guard = lock.lock();
		return !waiters.is_empty();
	}

	[[nodiscard]] bool is_pending() {
		IrqGuard irq_guard {};
		auto guard = lock.lock();
		return signaled_count > 0;
	}

private:
	DoubleList<Waitable, &Waitable::hook> waiters {};
	usize signaled_count {};
	Spinlock<void> lock {};
};

struct CallbackProducer {
	[[nodiscard]] usize add_callback(kstd::small_function<void()> callback);
	void remove_callback(usize index);

	void signal_one();
	void signal_all();

private:
	Spinlock<kstd::vector<kstd::small_function<void()>>> callbacks {};
};
