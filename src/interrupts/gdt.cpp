#include "gdt.hpp"

struct GdtEntry {
	inline GdtEntry(u32 base, u8 access_byte, u8 flags) {
		value = as<u64>(access_byte) << 40 | as<u64>(flags) << 52;

		u64 base_low = base & 0xFFFF;
		u64 base_middle = base >> 16 & 0xFF;
		u64 base_high = base >> 24 & 0xFF;

		value |= base_low << 16 | base_middle << 32 | base_high << 56;
	}
	constexpr GdtEntry(u64 base) : value {base} {} // NOLINT(google-explicit-constructor)

	u64 value;
};

GdtEntry gdt[7] {
		// Null
		{0, 0, 0},
		// Kernel Code
		{0, 0x9A, 0xA},
		// Kernel Data
		{0, 0x92, 0xC},
		// User Code
		{0, 0xFA, 0xA},
		// User Data
		{0, 0xF2, 0xC},
		// TSS
		{0},
		{0}
};

struct [[gnu::packed]] GdtDescriptor {
	u16 size;
	u64 offset;
};

extern "C" void load_gdt_asm(GdtDescriptor* desc);

void load_gdt(Tss* tss) {
	gdt[sizeof(gdt) / sizeof(*gdt) - 2] = {as<u32>(cast<usize>(tss)), 0x89, 0};
	gdt[sizeof(gdt) / sizeof(*gdt) - 1] = {cast<usize>(tss) >> 32};
	GdtDescriptor desc {.size = as<u16>(sizeof(gdt) - 1), .offset = cast<u64>(&gdt)};
	load_gdt_asm(&desc);
}