#pragma once
#include "types.hpp"
#include "utils.hpp"

struct GdtEntry {
	constexpr GdtEntry(u32 base, u8 access_byte, u8 flags) {
		value = as<u64>(access_byte) << 40 | as<u64>(flags) << 52;

		u64 base_low = base & 0xFFFF;
		u64 base_middle = base >> 16 & 0xFF;
		u64 base_high = base >> 24 & 0xFF;

		value |= base_low << 16 | base_middle << 32 | base_high << 56;
	}
	constexpr GdtEntry(u64 base) : value {base} {} // NOLINT(google-explicit-constructor)

	u64 value;
};

void load_gdt(GdtEntry* gdt, usize count);
