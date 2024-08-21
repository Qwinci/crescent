#pragma once
#include "dev/clock.hpp"
#include "arch/irq.hpp"

struct Cpu;

struct ArmTickSource : public TickSource {
	constexpr ArmTickSource() : TickSource {"arm tick source", 1} {}

	void init_on_cpu(Cpu* cpu);

	void oneshot(u64 us) override;
	void reset() override;
};

struct ArmClockSource : public ClockSource {
	constexpr ArmClockSource() : ClockSource {"arm clock source", 1} {}

	u64 get_ns() override;
};

extern ArmClockSource ARM_CLOCK_SOURCE;
