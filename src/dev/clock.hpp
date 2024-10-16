#pragma once
#include "double_list.hpp"
#include "utils/rw_spinlock.hpp"
#include "string_view.hpp"
#include "types.hpp"
#include "event.hpp"

struct ClockSource {
	constexpr ClockSource(kstd::string_view name, u64 frequency)
		: name {name}, frequency {frequency} {}

	DoubleListHook hook;

	virtual u64 get_ns() = 0;

	kstd::string_view name;
	u64 frequency;
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

static constexpr u64 NS_IN_US = 1000;
static constexpr u64 US_IN_MS = 1000;
static constexpr u64 US_IN_S = US_IN_MS * 1000;

template<typename F> requires requires(F f) {
	static_cast<bool>(f());
}
inline bool with_timeout(F f, usize us) {
	auto guard = CLOCK_SOURCE.lock_read();
	assert(guard && "no clock source available");
	auto source = *guard;
	auto start = source->get_ns();
	auto end = start + us * NS_IN_US;
	do {
		if (f()) {
			return true;
		}
	} while (source->get_ns() < end);
	return false;
}

inline u64 get_current_ns() {
	IrqGuard irq_guard {};
	auto guard = CLOCK_SOURCE.lock_read();
	return (*guard)->get_ns();
}
