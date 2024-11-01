#include "lapic.hpp"
#include "acpi/acpi.hpp"
#include "arch/cpu.hpp"
#include "arch/irq.hpp"
#include "arch/x86/cpu.hpp"
#include "dev/clock.hpp"
#include "mem/iospace.hpp"
#include "x86/irq.hpp"

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
	constexpr BitField<u32, u8> DELIVERY_MODE {8, 3};
	constexpr BitField<u32, bool> PENDING {12, 1};
	constexpr BitField<u32, bool> LEVEL {14, 1};
	constexpr BitField<u32, u8> DEST_SHORTHAND {18, 2};
	constexpr BitField<u32, u8> DEST {24, 8};

	constexpr u8 DEST_ALL_EXCL_SELF = 0b11;

	constexpr u8 DELIVERY_INIT = 0b101;
	constexpr u8 DELIVERY_SIPI = 0b110;
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

static void lapic_timer_calibrate(Cpu* cpu, bool initial) {
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

	if (initial) {
		cpu->lapic_timer.initialize(0xFFFFFFFF / ticks_in_us, ticks_in_us);
	}
	else {
		cpu->lapic_timer->max_us = 0xFFFFFFFF / ticks_in_us;
		cpu->lapic_timer->ticks_in_us = ticks_in_us;
	}

	SPACE.store(regs::DIVIDE_CONFIG, divide_config::DIV_BY_16);

	deregister_irq_handler(TMP_IRQ, &tmp_handler);
}

u8 LAPIC_IPI_START;

namespace {
	u32 WBINVD = 1 << 0;
	u32 WBINVD_FLUSH = 1 << 1;
}

static ManuallyDestroy<IrqHandler> LAPIC_HALT_HANDLER {{ // NOLINT
	.fn = [](IrqFrame*) {
		auto* cpu = get_current_thread()->cpu;

		if ((acpi::GLOBAL_FADT->flags & WBINVD_FLUSH) ||
			(acpi::GLOBAL_FADT->flags & WBINVD)) {
			asm volatile("wbinvd");
		}

		asm volatile(
			"push %%rax;"
			"push %%rbx;"
			"push %%rcx;"
			"push %%rdx;"
			"push %%rdi;"
			"push %%rsi;"
			"push %%rbp;"
			"push %%r8;"
			"push %%r9;"
			"push %%r10;"
			"push %%r11;"
			"push %%r12;"
			"push %%r13;"
			"push %%r14;"
			"push %%r15;"
			"mov %%rsp, (%0);"
			"leaq 1f(%%rip), %%rax;"
			"mov %%rax, (%1);"
			"movb $1, %2;"
			"0:"
			"hlt;"
			"jmp 0b;"
			"1:"
			"pop %%r15;"
			"pop %%r14;"
			"pop %%r13;"
			"pop %%r12;"
			"pop %%r11;"
			"pop %%r10;"
			"pop %%r9;"
			"pop %%r8;"
			"pop %%rbp;"
			"pop %%rsi;"
			"pop %%rdi;"
			"pop %%rdx;"
			"pop %%rcx;"
			"pop %%rbx;"
			"pop %%rax;"
			: : "r"(&cpu->saved_halt_rsp), "r"(&cpu->saved_halt_rip), "m"(cpu->ipi_ack) : "rax");

		return true;
	},
	.can_be_shared = false
}};

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

	LAPIC_IPI_START = x86_alloc_irq(static_cast<int>(Ipi::Max), false);
	assert(LAPIC_IPI_START);

	register_irq_handler(LAPIC_IPI_START + static_cast<int>(Ipi::Halt), &*LAPIC_HALT_HANDLER);
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

static inline u64 get_rdtsc() {
	u32 edx;
	u32 eax;
	asm volatile("rdtsc" : "=a"(eax), "=d"(edx));
	return static_cast<u64>(edx) << 32 | eax;
}

void lapic_boot_ap(u8 lapic_id, u32 boot_addr) {
	SPACE.store(regs::ICR1, icr::DEST(lapic_id));
	SPACE.store(
		regs::ICR0,
		icr::DELIVERY_MODE(icr::DELIVERY_INIT) |
		icr::LEVEL(true));

	u64 end = get_rdtsc() + 10000000;
	while (get_rdtsc() < end);

	SPACE.store(regs::ICR1, icr::DEST(lapic_id));
	SPACE.store(
		regs::ICR0,
		(boot_addr / 0x1000) |
		icr::DELIVERY_MODE(icr::DELIVERY_SIPI) |
		icr::LEVEL(true));
}

void lapic_init(Cpu* cpu, bool initial) {
	SPACE.store(regs::SVR, 0x1FF);
	SPACE.store(regs::TPR, 0);
	lapic_timer_calibrate(cpu, initial);
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
