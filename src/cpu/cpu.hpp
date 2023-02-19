#pragma once
#include "interrupts/gdt.hpp"
#include "interrupts/interrupts.hpp"
#include "types.hpp"
#include "utils/cpuid.hpp"

#ifdef WITH_SCHED_MULTI
#include "sched/sched.hpp"
#include "timer/timer.hpp"
#endif

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
	u64 apic_frequency {};
	u64 tsc_frequency {};
	Tss tss {.iopb = sizeof(Tss)};
	Task* current_task {};
	Task* sleeping_tasks {};
	SchedLevel levels[SCHED_MAX_LEVEL] {};
	u8 id {};
	Spinlock lock {};
	u8 reserved[3] {};
	atomic_uint_fast32_t thread_count = 0;
	Timer timer {};
};

extern CpuLocal* cpu_locals;
extern usize cpu_count;