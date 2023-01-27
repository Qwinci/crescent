#pragma once
#include "interrupts/gdt.hpp"
#include "interrupts/interrupts.hpp"
#include "types.hpp"
#include "utils/cpuid.hpp"

enum class Msr : u32 {
	FsBase = 0xC0000100,
	GsBase = 0xC0000101,
	KernelGsBase = 0xC0000102
};

struct Tss {
	u16 reserved1;
	u16 iopb;
	u32 reserved2[2];
	u32 ist7_high;
	u32 ist7_low;
	u32 ist6_high;
	u32 ist6_low;
	u32 ist5_high;
	u32 ist5_low;
	u32 ist4_high;
	u32 ist4_low;
	u32 ist3_high;
	u32 ist3_low;
	u32 ist2_high;
	u32 ist2_low;
	u32 ist1_high;
	u32 ist1_low;
	u32 reserved3[2];
	u32 rsp2_high;
	u32 rsp2_low;
	u32 rsp1_high;
	u32 rsp1_low;
	u32 rsp0_high;
	u32 rsp0_low;
	u32 reserved4;
};

static_assert(sizeof(Tss) == 104);

struct CpuLocal {
	inline CpuLocal() {
		id = cpuid(1).ebx >> 24;
	}
	inline explicit CpuLocal(u8 id) : id {id} {}

	CpuLocal* self {this};
	u64 apic_frequency {};
	u64 tsc_frequency {};
	Tss tss {.iopb = sizeof(Tss)};
	Idt idt {};
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
		{as<u32>(cast<usize>(&tss)), 0x89, 0},
		{as<u64>(cast<usize>(&tss) >> 32)}
	};
	u8 id {};
	u8 reserved[7] {};
};
static_assert(sizeof(CpuLocal) == 24 + sizeof(Tss) + sizeof(Idt) + sizeof(GdtEntry) * 7 + 8);

void set_cpu_local(CpuLocal* local);
CpuLocal* get_cpu_local();
void set_msr(Msr msr, u64 value);
u64 get_msr(Msr msr);