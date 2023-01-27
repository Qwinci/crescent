#pragma once
#include "types.hpp"
#include "utils.hpp"
#include "exceptions.hpp"

struct InterruptCtx {
	u64 rax, rbx, rcx, rdx, rdi, rsi, rbp, r8, r9, r10, r11, r12, r13, r14, r15;
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
private:
	IdtEntry interrupts[256];
};

using Handler = void (*)(InterruptCtx* ctx);

void set_exceptions();
void register_int_handler(u8 vec, Handler handler);
[[nodiscard]] u16 alloc_int_handler(Handler handler);
Handler get_int_handler(u8 vec);
void load_idt(Idt* idt);

static inline void enable_interrupts() {
	asm volatile("sti");
}

static inline void disable_interrupts() {
	asm volatile("sti");
}