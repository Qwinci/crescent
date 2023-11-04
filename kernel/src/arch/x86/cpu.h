#include "arch/cpu.h"
#include "arch/x86/dev/lapic_timer.h"
#include "arch/x86/sched/x86_task.h"
#include "tss.h"

typedef struct X86Cpu {
	struct X86Cpu* self;
	Cpu common;
	Tss tss;
	usize tsc_freq;
	LapicTimer lapic_timer;
	u8 apic_id;
} X86Cpu;

typedef enum : u32 {
	MSR_IA32_APIC_BASE = 0x1B,
	MSR_EFER = 0xC0000080,
	MSR_STAR = 0xC0000081,
	MSR_LSTAR = 0xC0000082,
	MSR_CSTAR = 0xC0000083,
	MSR_SFMASK = 0xC0000084,
	MSR_FSBASE = 0xC0000100,
	MSR_GSBASE = 0xC0000101,
	MSR_KERNELGSBASE = 0xC0000102
} Msr;

void x86_set_cpu_local(X86Task* task);
void x86_set_msr(Msr msr, u64 value);
u64 x86_get_msr(Msr msr);
X86Cpu* x86_get_cur_cpu();

typedef struct {
	usize xsave_area_size;
	bool rdrnd;
	bool xsave;
	bool avx;
	bool avx512;
	bool umip;
	bool smep;
	bool smap;
} CpuFeatures;
// this is used from assembly
static_assert(offsetof(CpuFeatures, smap) == 14);

extern CpuFeatures CPU_FEATURES;

static inline void xsave(u8* area, u64 mask) {
	assert((usize) area % 64 == 0);
	usize low = mask & 0xFFFFFFFF;
	usize high = mask >> 32;
	__asm__ volatile("xsave %0" : : "m"(*area), "a"(low), "d"(high) : "memory");
}

static inline void xrstor(u8* area, u64 mask) {
	assert((usize) area % 64 == 0);
	usize low = mask & 0xFFFFFFFF;
	usize high = mask >> 32;
	__asm__ volatile("xrstor %0" : : "m"(*area), "a"(low), "d"(high) : "memory");
}

static inline void wrxcr(u32 index, u64 value) {
	u32 low = value;
	u32 high = value >> 32;
	__asm__ volatile("xsetbv" : : "c"(index), "a"(low), "d"(high) : "memory");
}
