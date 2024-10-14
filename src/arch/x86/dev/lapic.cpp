#include "lapic.hpp"
#include "arch/x86/cpu.hpp"
#include "mem/iospace.hpp"
#include "x86/irq.hpp"
#include "arch/irq.hpp"
#include "dev/clock.hpp"
#include "arch/cpu.hpp"

namespace regs {
	constexpr BasicRegister<u32> EOI {0xB0};
	constexpr BasicRegister<u32> TPR {0x80};
	constexpr BasicRegister<u32> SVR {0xF0};
	constexpr BitRegister<u32> ICR0 {0x300};
	constexpr BitRegister<u32> ICR1 {0x310};
	constexpr BasicRegister<u32> LVT_TIMER {0x320};
	constexpr BasicRegister<u32> INIT_COUNT {0x380};
	constexpr BasicRegister<u32> CURR_COUNT {0x390};
	constexpr BasicRegister<u32> DIVIDE_CONFIG {0x3E0};
}

namespace icr {
	constexpr BitField<u32, u8> VECTOR {0, 8};
	constexpr BitField<u32, bool> PENDING {12, 1};
	constexpr BitField<u32, bool> LEVEL {14, 1};
	constexpr BitField<u32, u8> DEST_SHORTHAND {18, 2};
	constexpr BitField<u32, u8> DEST {24, 8};

	constexpr u8 DEST_ALL_EXCL_SELF = 0b11;
}

namespace divide_config {
	constexpr u32 DIV_BY_2 = 0b0000;
	constexpr u32 DIV_BY_4 = 0b0001;
	constexpr u32 DIV_BY_8 = 0b0010;
	constexpr u32 DIV_BY_16 = 0b0011;
	constexpr u32 DIV_BY_32 = 0b1000;
	constexpr u32 DIV_BY_64 = 0b1001;
	constexpr u32 DIV_BY_128 = 0b1010;
	constexpr u32 DIV_BY_1 = 0b1011;
}

namespace lvt_timer {
	constexpr BitField<u32, u8> MODE {17, 2};

	constexpr u8 MODE_ONE_SHOT = 0b00;
	constexpr u8 MODE_PERIODIC = 0b01;
	constexpr u8 MODE_TSC_DEADLINE = 0b10;
}

namespace lvt {
	constexpr BitField<u32, u8> VECTOR {0, 8};
	constexpr BitField<u32, bool> MASK {16, 1};
}

namespace {
	IoSpace SPACE {};
	u32 TMP_IRQ {};
	u32 TIMER_IRQ {};
	ManuallyInit<IrqHandler> LAPIC_IRQ_HANDLER;
}

static void lapic_timer_calibrate(Cpu* cpu) {
	SPACE.store(regs::DIVIDE_CONFIG, divide_config::DIV_BY_16);

	IrqHandler tmp_handler {
		.fn = [](IrqFrame*) {
			return true;
		},
		.can_be_shared = false
	};
	register_irq_handler(TMP_IRQ, &tmp_handler);

	SPACE.store(
		regs::LVT_TIMER,
		lvt::VECTOR(TMP_IRQ) | lvt_timer::MODE(lvt_timer::MODE_ONE_SHOT));
	SPACE.store(regs::INIT_COUNT, 0xFFFFFFFF);

	mdelay(10);

	SPACE.store(regs::LVT_TIMER, lvt::MASK(true));
	usize ticks_in_10ms = (0xFFFFFFFF - SPACE.load(regs::CURR_COUNT));
	usize ticks_in_ms = ticks_in_10ms / 10;
	usize ticks_in_us = ticks_in_ms / US_IN_MS;

	if (!ticks_in_us) {
		ticks_in_us = 1;
	}

	cpu->lapic_timer.initialize(0xFFFFFFFF / ticks_in_us, ticks_in_us);

	SPACE.store(regs::DIVIDE_CONFIG, divide_config::DIV_BY_16);

	deregister_irq_handler(TMP_IRQ, &tmp_handler);
}

void lapic_first_init() {
	SPACE.set_phys(msrs::IA32_APIC_BASE.read() & ~0xFFF, 0x1000);
	assert(SPACE.map(CacheMode::Uncached));
	TIMER_IRQ = x86_alloc_irq(1, false);
	assert(TIMER_IRQ);
	TMP_IRQ = x86_alloc_irq(1, false);
	assert(TMP_IRQ);

	LAPIC_IRQ_HANDLER.initialize(IrqHandler {
		.fn = &LapicTickSource::on_irq,
		.can_be_shared = false
	});
	register_irq_handler(TIMER_IRQ, &*LAPIC_IRQ_HANDLER);
}

void lapic_init_finalize() {
	x86_dealloc_irq(TMP_IRQ, 1, false);
}

void lapic_ipi(u8 vec, u8 dest) {
	SPACE.store(regs::ICR1, icr::DEST(dest));
	SPACE.store(regs::ICR0, icr::VECTOR(vec) | icr::LEVEL(true));
	while (SPACE.load(regs::ICR0) & icr::PENDING);
}

void lapic_ipi_all(u8 vec) {
	SPACE.store(regs::ICR0, icr::VECTOR(vec) | icr::LEVEL(true) |
		icr::DEST_SHORTHAND(icr::DEST_ALL_EXCL_SELF));
	while (SPACE.load(regs::ICR0) & icr::PENDING);
}

void lapic_init(Cpu* cpu) {
	SPACE.store(regs::SVR, 0x1FF);
	SPACE.store(regs::TPR, 0);
	lapic_timer_calibrate(cpu);
	cpu->cpu_tick_source = &*cpu->lapic_timer;
}

void lapic_eoi() {
	SPACE.store(regs::EOI, 0);
}

void LapicTickSource::oneshot(u64 us) {
	u64 value = ticks_in_us * us;
	SPACE.store(
		regs::LVT_TIMER,
		lvt::VECTOR(TIMER_IRQ) | lvt_timer::MODE(lvt_timer::MODE_ONE_SHOT));
	SPACE.store(regs::INIT_COUNT, value);
}

void LapicTickSource::reset() {
	IrqGuard irq_guard {};
	SPACE.store(regs::LVT_TIMER, lvt::MASK(true));
	SPACE.store(regs::INIT_COUNT, 0);
}

bool LapicTickSource::on_irq(struct IrqFrame*) {
	//println("[kernel][x86]: lapic irq on cpu ", get_current_thread()->cpu->number);
	auto lapic = get_current_thread()->cpu->cpu_tick_source;
	lapic->callback_producer.signal_all();
	return true;
}
