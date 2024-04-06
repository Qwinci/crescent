#include "gdt.hpp"
#include "tss.hpp"

static constexpr u64 gdt_entry(u32 base, u8 access, u8 flags, u32 limit) {
	return static_cast<u64>(limit & 0xFFFF) |
		(static_cast<u64>(limit >> 16) << 48) |
		(static_cast<u64>(access) << 40) |
		(static_cast<u64>(flags) << 52) |
		(base & 0xFFFF) << 16 |
		(static_cast<u64>(base >> 16 & 0xFF) << 32) |
		(static_cast<u64>(base >> 24 & 0xFF) << 56);
}

namespace {
	u64 GDT[8] {
		gdt_entry(0, 0, 0, 0),
		// kernel code
		gdt_entry(0, 0x9A, 0xA, 0),
		// kernel data
		gdt_entry(0, 0x92, 0xC, 0),
		// padding for syscall
		gdt_entry(0, 0, 0, 0),
		// user data
		gdt_entry(0, 0xF2, 0xC, 0),
		// user code
		gdt_entry(0, 0xFA, 0xA, 0),
		0,
		0
	};
}

struct [[gnu::packed]] Gdtr {
	u16 size;
	u64 offset;
};

extern "C" void x86_load_gdt_asm(Gdtr* gdtr);

void x86_load_gdt(Tss* tss) {
	auto tss_addr = reinterpret_cast<usize>(tss);
	GDT[sizeof(GDT) / 8 - 2] = gdt_entry(tss_addr, 0x89, 0, sizeof(Tss) - 1);
	GDT[sizeof(GDT) / 8 - 1] = tss_addr >> 32;
	Gdtr gdtr {.size = sizeof(GDT) - 1, .offset = reinterpret_cast<u64>(GDT)};
	x86_load_gdt_asm(&gdtr);
}
