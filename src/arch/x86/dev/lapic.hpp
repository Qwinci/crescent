#pragma once
#include "types.hpp"
#include "dev/clock.hpp"

struct LapicTickSource final : TickSource {
	constexpr LapicTickSource(u64 max_us, u64 ticks_in_us) : TickSource {"lapic", max_us}, ticks_in_us {ticks_in_us} {}

	void oneshot(u64 us) override;
	void reset() override;

	static bool on_irq(struct IrqFrame*);

	u64 ticks_in_us;
};

struct Cpu;

void lapic_first_init();
void lapic_init(Cpu* cpu, bool initial);
void lapic_eoi();

void lapic_ipi(u8 vec, u8 dest);
void lapic_ipi_all(u8 vec);

void lapic_boot_ap(u8 lapic_id, u32 boot_addr);
