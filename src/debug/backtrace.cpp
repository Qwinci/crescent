#include "backtrace.hpp"
#include "console.hpp"
#include "types.hpp"

struct Frame {
	Frame* rbp;
	u64 rip;
};

void debug_backtrace(bool lock) {
	Frame* frame;
	asm volatile("mov %0, rbp" : "=rm"(frame));

	if (lock) {
		println("stack trace:");
	}
	else {
		println_nolock("stack trace:");
	}
	usize i = 0;
	while (frame && i < 5) {
		if (lock) {
			println((void*) frame->rip);
		}
		else {
			println_nolock((void*) frame->rip);
		}
		frame = frame->rbp;
		i += 1;
	}
}