#include "event.hpp"
#include "utils/irq_guard.hpp"
#include "arch/cpu.hpp"

void Event::reset() {
	IrqGuard irq_guard {};
	auto guard = waiters.lock();
	signaled = false;
}

void Event::wait() {
	auto current = get_current_thread();
	IrqGuard irq_guard {};
	{
		auto guard = waiters.lock();
		if (!signaled) {
			guard->push(current);
		}
		else {
			return;
		}
	}

	current->cpu->scheduler.block();
}

bool Event::wait_with_timeout(u64 max_us) {
	auto current = get_current_thread();
	IrqGuard irq_guard {};
	{
		auto guard = waiters.lock();
		if (!signaled) {
			guard->push(current);
		}
		else {
			return true;
		}
	}

	current->cpu->scheduler.sleep(max_us);
	auto guard = waiters.lock();
	if (!signaled) {
		guard->remove(current);
		return false;
	}
	else {
		return true;
	}
}

void Event::signal_one() {
	IrqGuard irq_guard {};
	auto guard = waiters.lock();
	signaled = true;
	if (!guard->is_empty()) {
		auto thread = guard->front();
		thread->cpu->scheduler.unblock(thread);
		guard->remove(thread);
	}
}

void Event::signal_all() {
	IrqGuard irq_guard {};
	auto guard = waiters.lock();
	signaled = true;
	for (auto& thread : *guard) {
		thread.cpu->scheduler.unblock(&thread);
	}
	guard->clear();
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
