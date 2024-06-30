#pragma once
#include "double_list.hpp"
#include "utils/rw_spinlock.hpp"
#include "string_view.hpp"
#include "types.hpp"
#include "event.hpp"

struct ClockSource {
	constexpr ClockSource(kstd::string_view name, u64 ticks_in_us) : name {name}, ticks_in_us {ticks_in_us} {}

	DoubleListHook hook;

	virtual u64 get() = 0;

	kstd::string_view name;
	u64 ticks_in_us;
};

struct TickSource {
	constexpr TickSource(kstd::string_view name, u64 max_us) : name {name}, max_us {max_us} {}

	virtual void oneshot(u64 us) = 0;
	virtual void reset() = 0;

	kstd::string_view name;
	u64 max_us;

	CallbackProducer callback_producer {};
};

void clock_source_register(ClockSource* source);
void clock_source_deregister(ClockSource* source);

void mdelay(usize ms);
void udelay(usize us);

extern RwSpinlock<ClockSource*> CLOCK_SOURCE;

static constexpr u64 US_IN_MS = 1000;
static constexpr u64 US_IN_S = US_IN_MS * 1000;

template<typename F> requires requires(F f) {
	static_cast<bool>(f());
}
inline bool with_timeout(F f, usize us) {
	auto guard = CLOCK_SOURCE.lock_read();
	assert(guard && "no clock source available");
	auto source = *guard;
	auto ticks_in_us = source->ticks_in_us;
	auto start = source->get();
	auto end = start + ticks_in_us * us;
	do {
		if (f()) {
			return true;
		}
	} while (source->get() < end);
	return false;
}
