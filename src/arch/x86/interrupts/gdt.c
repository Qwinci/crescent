#include "gdt.h"
#include "types.h"
#include "arch/x86/tss.h"

#define GDT_ENTRY(base, access_byte, flags, limit) \
	((u64) (limit) & 0xFFFF | \
	 (u64) (limit) >> 16 << 48 | \
	 (u64) (access_byte) << 40 | \
	 (u64) (flags) << 52 | \
	 ((u64) (base) & 0xFFFF) << 16 | \
	 ((u64) (base) >> 16 & 0xFF) << 32 | \
	 ((u64) (base) >> 24 & 0xFF) << 56)

static u64 gdt[8] = {
	// Null
	GDT_ENTRY(0, 0, 0, 0),
	// Kernel Code
	GDT_ENTRY(0, 0x9A, 0xA, 0),
	// Kernel Data
	GDT_ENTRY(0, 0x92, 0xC, 0),
	// User Null
	GDT_ENTRY(0, 0, 0, 0),
	// User Data
	GDT_ENTRY(0, 0xF2, 0xC, 0),
	// User Code
	GDT_ENTRY(0, 0xFA, 0xA, 0),
	// TSS
	0,
	0
};

typedef struct [[gnu::packed]] {
	u16 size;
	u64 offset;
} Gdtr;

extern void x86_load_gdt_asm(Gdtr* gdtr);

void x86_load_gdt(Tss* tss) {
	gdt[sizeof(gdt) / sizeof(*gdt) - 2] = GDT_ENTRY((usize) tss, 0x89, 0, sizeof(Tss) - 1);
	gdt[sizeof(gdt) / sizeof(*gdt) - 1] = (usize) tss >> 32;
	Gdtr gdtr = {.size = sizeof(gdt) - 1, .offset = (u64) &gdt};
	x86_load_gdt_asm(&gdtr);
}