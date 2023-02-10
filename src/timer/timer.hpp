#pragma once
#include "types.hpp"

static inline void udelay(u64 us) {
	extern void (*udelay_ptr)(u64 us);
	udelay_ptr(us);
}

extern u16 timer_vec;

void init_timers(const void* rsdp);

class Timer {
public:
	void create_timer(usize timestamp, void (*fn)());
	void trigger_timers(usize current_time);
private:
	struct Node {
		usize timestamp;
		void (*fn)();
		Node* next;
	};
	Node* timers {};
	Node* timers_end {};
};