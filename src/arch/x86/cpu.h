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