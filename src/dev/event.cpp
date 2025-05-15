#include "event.hpp"
#include "utils/irq_guard.hpp"
#include "arch/cpu.hpp"

void Event::reset() {
	IrqGuard irq_guard {};
	auto guard = lock.lock();
	signaled_count = 0;
}

void Event::wait(bool consume) {
	IrqGuard irq_guard {};

	auto current = get_current_thread();

	Waitable waitable {
		.thread = current,
		.in_list = false
	};

	while (true) {
		{
			auto guard = lock.lock();
			if (signaled_count) {
				if (waitable.in_list) {
					waiters.remove(&waitable);
				}
				if (consume) {
					--signaled_count;
				}
				return;
			}
			if (!waitable.in_list) {
				waiters.push(&waitable);
				waitable.in_list = true;
			}
		}
		current->cpu->scheduler.block();
	}
}

usize Event::wait_any(Event** events, usize count, u64 max_ns) {
	IrqGuard irq_guard {};

	auto current = get_current_thread();

	kstd::vector<Waitable> waitables;
	waitables.resize(count);
	for (auto& waitable : waitables) {
		waitable.thread = current;
	}

	for (usize i = 0; i < count; ++i) {
		auto& event = events[i];
		auto guard = event->lock.lock();
		if (event->signaled_count) {
			for (usize j = 0; j < i; ++j) {
				auto& ev = events[j];
				auto ev_guard = ev->lock.lock();
				if (waitables[j].in_list) {
					ev->waiters.remove(&waitables[j]);
				}
			}

			--event->signaled_count;
			current->dont_block = false;
			return i + 1;
		}

		event->waiters.push(&waitables[i]);
	}

	if (max_ns != UINT64_MAX) {
		current->cpu->scheduler.sleep(max_ns);
	}
	else {
		current->cpu->scheduler.block();
	}

	for (usize i = 0; i < count; ++i) {
		auto& event = events[i];
		auto guard = event->lock.lock();

		if (waitables[i].in_list) {
			event->waiters.remove(&waitables[i]);
		}

		if (event->signaled_count) {
			--event->signaled_count;

			for (usize j = i + 1; j < count; ++j) {
				auto& ev = events[j];
				auto ev_guard = ev->lock.lock();
				if (waitables[j].in_list) {
					ev->waiters.remove(&waitables[j]);
				}
			}

			current->dont_block = false;
			return i + 1;
		}
	}

	current->dont_block = false;
	return 0;
}

bool Event::wait_with_timeout(u64 max_ns) {
	IrqGuard irq_guard {};

	auto current = get_current_thread();

	Waitable waitable {
		.thread = current,
		.in_list = true
	};

	{
		auto guard = lock.lock();
		if (signaled_count) {
			--signaled_count;
			return true;
		}

		waiters.push(&waitable);
	}

	current->cpu->scheduler.sleep(max_ns);

	auto guard = lock.lock();
	if (waitable.in_list) {
		waiters.remove(&waitable);
	}

	current->dont_block = false;
	if (signaled_count) {
		--signaled_count;
		return true;
	}
	else {
		return false;
	}
}

void Event::signal_one() {
	IrqGuard irq_guard {};
	auto guard = lock.lock();
	++signaled_count;

	if (!waiters.is_empty()) {
		auto waitable = waiters.pop_front();
		waitable->in_list = false;
		auto thread = waitable->thread;
		auto thread_guard = thread->sched_lock.lock();
		thread->cpu->scheduler.unblock(thread, true, false);
	}
}

void Event::signal_one_if_not_pending() {
	IrqGuard irq_guard {};
	auto guard = lock.lock();
	if (signaled_count) {
		return;
	}
	++signaled_count;

	if (!waiters.is_empty()) {
		auto waitable = waiters.pop_front();
		waitable->in_list = false;
		auto thread = waitable->thread;
		auto thread_guard = thread->sched_lock.lock();
		thread->cpu->scheduler.unblock(thread, true, false);
	}
}

void Event::signal_all() {
	IrqGuard irq_guard {};
	auto guard = lock.lock();
	++signaled_count;

	for (auto& waitable : waiters) {
		auto thread = waitable.thread;
		auto thread_guard = thread->sched_lock.lock();
		thread->cpu->scheduler.unblock(thread, true, false);
		waitable.in_list = false;
	}
	waiters.clear();
}

void Event::signal_count(usize count) {
	IrqGuard irq_guard {};
	auto guard = lock.lock();
	signaled_count += count;

	for (auto& waitable : waiters) {
		auto thread = waitable.thread;
		auto thread_guard = thread->sched_lock.lock();
		thread->cpu->scheduler.unblock(thread, true, false);

		if (--count == 0) {
			break;
		}

		waiters.remove(&waitable);
		waitable.in_list = false;
	}
}

usize CallbackProducer::add_callback(kstd::small_function<void()> callback) {
	auto guard = callbacks.lock();
	auto index = guard->size();
	guard->push(std::move(callback));
	return index;
}

void CallbackProducer::remove_callback(usize index) {
	auto guard = callbacks.lock();
	assert(index < guard->size());
	guard->remove(index);
}

void CallbackProducer::signal_one() {
	auto guard = callbacks.lock();
	if (auto front = guard->front()) {
		(*front)();
	}
}

void CallbackProducer::signal_all() {
	auto guard = callbacks.lock();
	for (auto& callback : *guard) {
		callback();
	}
}
