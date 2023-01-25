#include "lapic.hpp"
#include "console.hpp"
#include "cpu/cpu.hpp"
#include "timer/timer.hpp"
#include "utils.hpp"

usize Lapic::base = 0;

u32 Lapic::read(Reg reg) {
	return *cast<volatile u32*>(base + as<u32>(reg));
}

void Lapic::write(Reg reg, u32 value) {
	*cast<volatile u32*>(base + as<u32>(reg)) = value;
}

constexpr u32 ONESHOT = 0;
constexpr u32 PERIODIC = 1 << 17;
constexpr u32 TSC = 2 << 17;

[[gnu::interrupt]] static void tmp_handler(InterruptFrame*) {
	Lapic::eoi();
}

void Lapic::calibrate_timer() {
	println("info: calibrating apic timer");

	write(Reg::DivConf, 3);
	auto irq = register_irq_handler(0, tmp_handler);
	write(Reg::LvtTimer, irq | ONESHOT);
	write(Lapic::Reg::InitCount, 0xFFFFFFFF);

	// wait 10ms
	udelay(10 * 1000);

	write(Reg::LvtTimer, INT_MASKED);
	auto ticks_in_1s = as<u64>(0xFFFFFFFF - read(Reg::CurrCount)) * 16 * 100;
	get_cpu_local()->apic_frequency = ticks_in_1s;

	println("info: apic frequency: ", ticks_in_1s);
}

void Lapic::start_oneshot(u64 frequency, u8 irq) {
	write(Reg::DivConf, 3);
	write(Reg::LvtTimer, (32 + irq) | ONESHOT);
	u64 value = get_cpu_local()->apic_frequency / 16 / frequency;
	if (value == 0) {
		panic("lapic oneshot frequency is too high");
	}
	write(Lapic::Reg::InitCount, value);
}

void Lapic::start_periodic(u64 frequency) {
	write(Reg::DivConf, 3);
	write(Reg::LvtTimer, 32 | PERIODIC);
	write(Lapic::Reg::InitCount, get_cpu_local()->apic_frequency / 16 / frequency);
}

void Lapic::eoi() {
	write(Reg::Eoi, 0);
}
