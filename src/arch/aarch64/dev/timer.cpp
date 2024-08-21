#include "timer.hpp"
#include "arch/cpu.hpp"
#include "arch/irq.hpp"
#include "utils/driver.hpp"

static bool on_irq(IrqFrame*);

namespace {
	u64 TICKS_PER_SECOND = 0;
	u64 TICKS_PER_MS = 0;
	u32 VIRT_IRQ = 0;
	ManuallyDestroy<IrqHandler> IRQ_HANDLER {{
		.fn = on_irq
	}};

	u64 get_system_counter() {
		u64 count;
		asm volatile("mrs %0, cntpct_el0" : "=r"(count));
		return count;
	}

	u64 get_virtual_system_counter() {
		u64 count;
		asm volatile("mrs %0, cntvct_el0" : "=r"(count));
		return count;
	}
}

void ArmTickSource::oneshot(u64 us) {
	auto current = get_virtual_system_counter();
	auto compare = current + TICKS_PER_MS * (us / US_IN_MS);
	asm volatile("msr cntv_cval_el0, %0" : : "r"(compare));
}

void ArmTickSource::reset() {
	asm volatile("msr cntv_cval_el0, %0" : : "r"(0xFFFFFFFFFFFFFFFF));
}

void ArmTickSource::init_on_cpu(Cpu* cpu) {
	cpu->cpu_tick_source = this;

	GIC->mask_irq(VIRT_IRQ, false);
	register_irq_handler(VIRT_IRQ, &*IRQ_HANDLER);
}

u64 ArmClockSource::get_ns() {
	return get_system_counter() * (US_IN_MS * NS_IN_US) / TICKS_PER_MS;
}

static bool on_irq(IrqFrame*) {
	auto cpu = get_current_thread()->cpu;
	cpu->arm_tick_source.reset();
	cpu->arm_tick_source.callback_producer.signal_all();
	return true;
}

static bool arm_timer_init(DtbNode& node) {
	println("[kernel][aarch64]: arm timer init");

	assert(node.irqs.size() >= 3);

	auto virt_irq = node.irqs[2];
	VIRT_IRQ = virt_irq.num;

	GIC->set_mode(virt_irq.num, virt_irq.mode);

	if (auto freq_opt = node.prop("clock-frequency")) {
		TICKS_PER_SECOND = freq_opt->read<u32>(0);
	}
	else {
		asm volatile("mrs %0, cntfrq_el0" : "=r"(TICKS_PER_SECOND));
	}
	TICKS_PER_MS = TICKS_PER_SECOND / 1000;

	ARM_CLOCK_SOURCE.frequency = TICKS_PER_SECOND;
	clock_source_register(&ARM_CLOCK_SOURCE);

	asm volatile("msr cntv_cval_el0, %0" : : "r"(0xFFFFFFFFFFFFFFFF));
	asm volatile("msr cntv_ctl_el0, %0" : : "r"(u64 {1}));

	return true;
}

static constexpr DtDriver DRIVER {
	.init = arm_timer_init,
	.compatible {"arm,armv8-timer", "arm,armv7-timer"},
	.depends {"gic"},
	.provides {"timer"}
};

DT_DRIVER(DRIVER);

ArmClockSource ARM_CLOCK_SOURCE {};
