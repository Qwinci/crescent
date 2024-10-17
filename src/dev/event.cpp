#include "event.hpp"
#include "utils/irq_guard.hpp"
#include "arch/cpu.hpp"

void Event::reset() {
	IrqGuard irq_guard {};
	auto guard = signaled_count.lock();
	*guard = 0;
}

void Event::wait() {
	IrqGuard irq_guard {};
	auto current = get_current_thread();

	while (true) {
		{
			auto guard = signaled_count.lock();
			if (*guard) {
				--*guard;
				return;
			}

			auto waiters_guard = waiters.lock();
			waiters_guard->push(current);
		}

		current->cpu->scheduler.block();
	}
}

bool Event::wait_with_timeout(u64 max_us) {
	IrqGuard irq_guard {};
	auto current = get_current_thread();

	{
		auto guard = signaled_count.lock();
		if (*guard) {
			--*guard;
			return true;
		}

		auto waiters_guard = waiters.lock();
		waiters_guard->push(current);
	}

	current->cpu->scheduler.sleep(max_us);
	auto guard = signaled_count.lock();
	if (*guard) {
		--*guard;
		return true;
	}
	else {
		auto waiters_guard = waiters.lock();
		waiters_guard->remove(current);
		return false;
	}
}

void Event::signal_one() {
	IrqGuard irq_guard {};
	auto guard = signaled_count.lock();
	++*guard;

	auto waiters_guard = waiters.lock();
	if (!waiters_guard->is_empty()) {
		auto thread = waiters_guard->front();
		auto thread_guard = thread->sched_lock.lock();
		if (thread->status != Thread::Status::Waiting &&
			thread->status != Thread::Status::Running) {
			thread->cpu->scheduler.unblock(thread, true, true);
		}
		waiters_guard->remove(thread);
	}
}

void Event::signal_one_if_not_pending() {
	IrqGuard irq_guard {};
	auto guard = signaled_count.lock();
	if (*guard) {
		return;
	}
	++*guard;

	auto waiters_guard = waiters.lock();
	if (!waiters_guard->is_empty()) {
		auto thread = waiters_guard->front();
		auto thread_guard = thread->sched_lock.lock();
		if (thread->status != Thread::Status::Waiting &&
			thread->status != Thread::Status::Running) {
			thread->cpu->scheduler.unblock(thread, true, true);
		}
		waiters_guard->remove(thread);
	}
}

void Event::signal_all() {
	IrqGuard irq_guard {};
	auto guard = signaled_count.lock();
	++*guard;

	auto waiters_guard = waiters.lock();
	for (auto& thread : *waiters_guard) {
		auto thread_guard = thread.sched_lock.lock();
		if (thread.status != Thread::Status::Waiting &&
			thread.status != Thread::Status::Running) {
			thread.cpu->scheduler.unblock(&thread, true, true);
		}
	}
	waiters_guard->clear();
}

void Event::signal_count(usize count) {
	IrqGuard irq_guard {};
	auto guard = signaled_count.lock();
	*guard += count;

	auto waiters_guard = waiters.lock();
	for (auto& thread : *waiters_guard) {
		auto thread_guard = thread.sched_lock.lock();
		thread.cpu->scheduler.unblock(&thread, true, true);
	}
	waiters_guard->clear();
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
