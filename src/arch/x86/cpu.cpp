#include "arch.hpp"
#include "cpu.hpp"

void set_msr(Msr msr, u64 value) {
	asm volatile("wrmsr" : : "a"(as<u32>(value & 0xFFFFFFFF)), "d"(as<u32>(value >> 32)), "c"(msr));
}

u64 get_msr(Msr msr) {
	u32 low, high;
	asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
	return as<u64>(low) | as<u64>(high) << 32;
}

void set_cpu_local(X86CpuLocal* local) {
	set_msr(Msr::GsBase, cast<u64>(local));
}

X86CpuLocal* get_cpu_local() {
	X86CpuLocal* local;
	asm volatile("mov %0, gs:0" : "=r"(local));
	return local;
}

CpuLocal* arch_get_cpu_local() {
	return &get_cpu_local()->common;
}

extern X86CpuLocal* cpu_locals;

CpuLocal* arch_get_cpu_local(usize cpu) {
	return &cpu_locals[cpu].common;
}
