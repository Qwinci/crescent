#pragma once
#include "cpu/cpu.hpp"

extern "C" void syscall_entry_asm();

constexpr usize USER_SEG_BASE = 0x18;

static inline void init_usermode() {
	auto efer = get_msr(Msr::Efer);
	efer |= 1;
	set_msr(Msr::Efer, efer);

	set_msr(Msr::LStar, cast<u64>(syscall_entry_asm));

	u64 star = 0x8ULL << 32 | USER_SEG_BASE << 48;
	set_msr(Msr::Star, star);
}