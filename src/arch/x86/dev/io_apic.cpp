#include "io_apic.hpp"
#include "mem/mem.hpp"
#include "sched/sched.hpp"
#include "arch/cpu.hpp"

namespace regs {
	static constexpr BasicRegister<u32> IOREGSEL {0x0};
	static constexpr BasicRegister<u32> IOWIN {0x10};

	static constexpr u8 IOAPICVER = 0x1;
}

u32 IoApicManager::IoApic::read(u8 reg) {
	space.store(regs::IOREGSEL, reg);
	return space.load(regs::IOWIN);
}

void IoApicManager::IoApic::write(u8 reg, u32 value) {
	space.store(regs::IOREGSEL, reg);
	space.store(regs::IOWIN, value);
}

void IoApicManager::register_io_apic(u32 phys, u32 gsi_base) {
	assert(io_apic_count != 16);
	auto& apic = io_apics[io_apic_count++];
	apic = {
		.space {IoSpace {to_virt<void>(phys)}},
		.gsi_base = gsi_base
	};
	apic.gsi_count = (apic.read(regs::IOAPICVER) >> 16 & 0xFF) + 1;
}

void IoApicManager::register_override(u8 irq, u32 gsi, bool active_low, bool level_triggered) {
	overrides[irq] = {
		.gsi = gsi,
		.used = true,
		.active_low = active_low,
		.level_triggered = level_triggered
	};
}

void IoApicManager::register_irq(u32 gsi, IoApicIrqInfo info) {
	u64 value = info.vec;
	value |= static_cast<u64>(info.delivery) << 8;
	value |= static_cast<u64>(info.polarity) << 13;
	value |= static_cast<u64>(info.trigger) << 15;
	value |= static_cast<u64>(get_current_thread()->cpu->lapic_id) << 56;

	for (usize i = 0; i < io_apic_count; ++i) {
		auto& apic = io_apics[i];
		if (apic.gsi_base <= gsi && gsi - apic.gsi_base < apic.gsi_count) {
			u8 entry = gsi - apic.gsi_base;
			apic.write(0x10 + entry * 2, value);
			apic.write(0x10 + entry * 2 + 1, value >> 32);
			return;
		}
	}
}

void IoApicManager::register_isa_irq(u8 irq, u8 vec) {
	auto& override = overrides[irq];
	IoApicIrqInfo info {};
	info.vec = vec;
	u32 gsi;
	if (override.used) {
		info.polarity = override.active_low ? IoApicPolarity::ActiveLow : IoApicPolarity::ActiveHigh;
		info.trigger = override.level_triggered ? IoApicTrigger::Level : IoApicTrigger::Edge;
		gsi = override.gsi;
	}
	else {
		gsi = irq;
	}

	register_irq(gsi, info);
}

IoApicManager IO_APIC {};
