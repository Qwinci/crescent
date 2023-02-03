#pragma once
#include "types.hpp"
#include "utils.hpp"
#include "exceptions.hpp"

struct InterruptCtx {
	u64 r15, r14, r13, r12, r11, r10, r9, r8, rbp, rsi, rdi, rdx, rcx, rbx, rax;
	u64 vec;
	u64 error;
	u64 ip;
	u64 cs;
	u64 flags;
	u64 sp;
	u64 ss;
};

struct IdtEntry {
	constexpr IdtEntry() = default;
	constexpr IdtEntry(void* fn, u8 selector, u8 ist, bool trap, u8 dpl)
		: selector {selector}, ist {ist} {
		auto addr = cast<usize>(fn);
		offset0 = addr & 0xFFFF;
		offset1 = addr >> 16;
		offset2 = addr >> 32;
		flags = (trap ? 0xF : 0xE) | dpl << 5 | 1 << 7;
	}

	u16 offset0 {};
	u16 selector {};
	u8 ist {};
	u8 flags {};
	u16 offset1 {};
	u32 offset2 {};
	u32 reserved {};
};

struct Idt {
	Idt();
	IdtEntry interrupts[256];
};

using Handler = void (*)(InterruptCtx* ctx);

void set_exceptions();
void register_int_handler(u8 vec, Handler handler);
[[nodiscard]] u16 alloc_int_handler(Handler handler);
Handler get_int_handler(u8 vec);
void load_idt();

static inline void enable_interrupts() {
	asm volatile("sti");
}

static inline void disable_interrupts() {
	asm volatile("cli");
}

static inline bool disable_interrupts_with_prev() {
	u16 flags;
	asm volatile("pushfw; pop %0; cli" : "=r"(flags));
	return flags & 1 << 9;
}

static inline bool enter_critical() {
	return disable_interrupts_with_prev();
}

static inline void leave_critical(bool flags) {
	if (flags) {
		enable_interrupts();
	}
}