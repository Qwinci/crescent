#pragma once
#include "interrupts/gdt.hpp"
#include "interrupts/interrupts.hpp"
#include "types.hpp"
#include "utils/cpuid.hpp"

#ifdef WITH_SCHED_MULTI
#include "sched/sched.hpp"
#endif

enum class Msr : u32 {
	Ia32ApicBase = 0x1B,
	Efer = 0xC0000080,
	Star = 0xC0000081,
	LStar = 0xC0000082,
	CStar = 0xC0000083,
	SFMask = 0xC0000084,
	FsBase = 0xC0000100,
	GsBase = 0xC0000101,
	KernelGsBase = 0xC0000102
};

struct Tss {
	u32 reserved1;
	u32 rsp0_low;
	u32 rsp0_high;
	u32 rsp1_low;
	u32 rsp1_high;
	u32 rsp2_low;
	u32 rsp2_high;
	u32 reserved2[2];
	u32 ist1_low;
	u32 ist1_high;
	u32 ist2_low;
	u32 ist2_high;
	u32 ist3_low;
	u32 ist3_high;
	u32 ist4_low;
	u32 ist4_high;
	u32 ist5_low;
	u32 ist5_high;
	u32 ist6_low;
	u32 ist6_high;
	u32 ist7_low;
	u32 ist7_high;
	u32 reserved3[2];
	u16 reserved4;
	u16 iopb;
};

static_assert(sizeof(Tss) == 104);

struct CpuLocal {
	inline CpuLocal() {
		id = cpuid(1).ebx >> 24;
	}
	inline explicit CpuLocal(u8 id) : id {id} {}

	CpuLocal* self {this};
	void* user_stack;
	u64 apic_frequency {};
	u64 tsc_frequency {};
	Tss tss {.iopb = sizeof(Tss)};
	u8 id {};
	u8 reserved[7] {};
};

void set_cpu_local(CpuLocal* local);
CpuLocal* get_cpu_local();
void set_msr(Msr msr, u64 value);
u64 get_msr(Msr msr);