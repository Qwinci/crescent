#include "timer.hpp"
#include "arch/cpu.hpp"

void ArmTickSource::init_on_cpu(Cpu* cpu) {
	cpu->cpu_tick_source = this;
}

void ArmClockSource::init() {
	clock_source_register(this);
}

ArmClockSource ARM_CLOCK_SOURCE {};
