#include "gdt.hpp"

struct [[gnu::packed]] GdtDescriptor {
	u16 size;
	u64 offset;
};

extern "C" void load_gdt_asm(GdtDescriptor* desc);

void load_gdt(GdtEntry* gdt, usize count) {
	GdtDescriptor desc {.size = as<u16>(count * sizeof(GdtEntry) - 1), .offset = cast<u64>(gdt)};
	load_gdt_asm(&desc);
}