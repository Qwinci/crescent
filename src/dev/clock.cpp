#include "clock.hpp"
#include "stdio.hpp"
#include "assert.hpp"

namespace {
	Spinlock<DoubleList<ClockSource, &ClockSource::hook>> CLOCK_SOURCES;
}

void clock_source_register(ClockSource* source) {
	println("[kernel][clock]: registering clock source ", source->name);

	auto guard = CLOCK_SOURCES.lock();
	if (guard->is_empty()) {
		guard->push(source);
		*CLOCK_SOURCE.lock_write() = source;
		println("[kernel][clock]: switched to clock source ", source->name);
	}
	else {
		if (source->ticks_in_us > guard->front()->ticks_in_us) {
			guard->push_front(source);
			*CLOCK_SOURCE.lock_write() = source;
			println("[kernel][clock]: switched to clock source ", source->name);
		}
		else {
			guard->push(source);
		}
	}
}

void clock_source_deregister(ClockSource* source) {
	println("[kernel][clock]: deregistering clock source ", source->name);

	auto clock_sources_guard = CLOCK_SOURCES.lock();
	clock_sources_guard->remove(source);

	auto clock_source_guard = CLOCK_SOURCE.lock_write();
	if (clock_source_guard == source) {
		if (clock_sources_guard->is_empty()) {
			*clock_source_guard = nullptr;
			println("[kernel][clock]: warning: no clock source configured!");
		}
		else {
			*clock_source_guard = clock_sources_guard->front();
			println("[kernel][clock]: switched to clock source ", (*clock_source_guard)->name);
		}
	}
}

void mdelay(usize ms) {
	auto guard = CLOCK_SOURCE.lock_read();
	assert(guard && "no clock source available");
	auto source = *guard;
	auto ticks_in_ms = source->ticks_in_us * US_IN_MS;
	auto start = source->get();
	auto end = start + ticks_in_ms * ms;
	while (source->get() < end);
}

void udelay(usize us) {
	auto guard = CLOCK_SOURCE.lock_read();
	assert(guard && "no clock source available");
	auto source = *guard;
	auto start = source->get();
	auto end = start + source->ticks_in_us * us;
	while (source->get() < end);
}

RwSpinlock<ClockSource*> CLOCK_SOURCE {};
