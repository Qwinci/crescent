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
[[maybe_unused]] constexpr u32 TSC = 2 << 17;

static void tmp_handler(InterruptCtx*) {
	Lapic::eoi();
}

void Lapic::calibrate_timer() {
	write(Reg::DivConf, 3);

	Handler old_handler = nullptr;
	if (!timer_vec) {
		timer_vec = alloc_int_handler(tmp_handler);
	}
	else {
		old_handler = get_int_handler(timer_vec);
		register_int_handler(timer_vec, tmp_handler);
	}

	write(Reg::LvtTimer, timer_vec | ONESHOT);
	write(Lapic::Reg::InitCount, 0xFFFFFFFF);

	// wait 10ms
	udelay(10 * 1000);

	write(Reg::LvtTimer, INT_MASKED);
	auto ticks_in_1s = as<u64>(0xFFFFFFFF - read(Reg::CurrCount)) * 16 * 100;
	get_cpu_local()->apic_frequency = ticks_in_1s;

	register_int_handler(timer_vec, old_handler);
}

void Lapic::start_oneshot(u64 frequency, u8 vec) {
	write(Reg::DivConf, 3);
	write(Reg::LvtTimer, vec | ONESHOT);
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

Lapic::Msg Lapic::current_msg {Msg::None};
static Spinlock msg_lock {};
static u8 ipi_vec = 0;

void ipi_handler(InterruptCtx*) {
	disable_interrupts();
	switch (Lapic::current_msg) {
		case Lapic::Msg::None:
			break;
		case Lapic::Msg::Halt:
		{
			Lapic::eoi();
			panic("received halt msg on cpu ", Fmt::Dec, get_cpu_local()->id);
		}
	}
	Lapic::eoi();
}

void Lapic::init() {
	write(Reg::SpuriousInt, 0xFF | 0x100);
	write(Reg::TaskPriority, 0);
}

void Lapic::send_ipi_all(Msg msg) {
	msg_lock.lock();
	if (!ipi_vec) {
		ipi_vec = alloc_int_handler(ipi_handler);
	}
	current_msg = msg;
	u32 value = as<u32>(ipi_vec) | 1 << 15 | 3 << 18;
	write(Reg::IntCmdBase, value);

	while (read(Reg::IntCmdBase) & 1 << 12) {
		__builtin_ia32_pause();
	}

	msg_lock.unlock();
}
