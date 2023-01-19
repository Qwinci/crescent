#include "io_apic.hpp"
#include "console.hpp"
#include "utils.hpp"

IoApic IoApic::apics[16];
u8 IoApic::io_apic_count = 0;
IoApic::Override IoApic::overrides[16];

void IoApic::register_override(u8 irq, u8 global_base, bool active_low, bool level_triggered) {
	overrides[irq] = {true, global_base, active_low, level_triggered};
}

u32 IoApic::read(u8 apic, u8 reg) {
	*cast<volatile u32*>(apics[apic].base) = reg;
	return *cast<volatile u32*>(apics[apic].base + 0x10);
}

void IoApic::write(u8 apic, u8 reg, u32 value) {
	*cast<volatile u32*>(apics[apic].base) = reg;
	*cast<volatile u32*>(apics[apic].base + 0x10) = value;
}

IoApic::IsaRedirInfo IoApic::get_isa_irq_info(u8 irq) {
	const Override& override = overrides[irq];
	if (override.enabled) {
		for (u8 i = 0; i < io_apic_count; ++i) {
			const auto& apic = apics[i];
			if (apic.int_base <= override.global_base &&
				override.global_base - apic.int_base < apic.irq_count) {
				return {
						.io_apic_index = i,
						.io_apic_entry = as<u8>(override.global_base - apic.int_base),
						.active_low = override.active_low,
						.level_triggered = override.level_triggered
				};
			}
		}
		unreachable();
	}
	for (u8 i = 0; i < io_apic_count; ++i) {
		const auto& apic = apics[i];
		if (apic.int_base == 0) {
			return {
					.io_apic_index = i,
					.io_apic_entry = irq,
					.active_low = false,
					.level_triggered = false
			};
		}
	}

	unreachable();
}

i16 IoApic::get_free_irq() {
	for (u8 i = 0; i < io_apic_count; ++i) {
		auto& apic = apics[i];
		for (u8 j = 0; j < apic.irq_count; ++j) {
			if ((apic.used_irqs & 1 << j) == 0) {
				apic.used_irqs |= 1 << j;
				return j;
			}
		}
	}
	return -1;
}

bool IoApic::is_entry_free(u8 io_apic_irq) {
	for (u8 i = 0; i < io_apic_count; ++i) {
		const auto& apic = apics[i];
		if (apic.int_base <= io_apic_irq &&
			io_apic_irq - apic.int_base < apic.irq_count) {
			return apic.used_irqs & 1 << (io_apic_irq - apic.int_base);
		}
	}
	return false;
}

void IoApic::register_irq(u8 io_apic_irq, RedirEntry entry) {
	u64 e = entry.vector;
	e |= as<u64>(entry.delivery_mode) << 8;
	e |= as<u64>(entry.dest_mode) << 11;
	e |= as<u64>(entry.polarity) << 13;
	e |= as<u64>(entry.trigger_mode) << 15;
	e |= as<u64>(entry.mask) << 16;
	e |= as<u64>(entry.dest) << 56;

	for (u8 i = 0; i < io_apic_count; ++i) {
		auto& apic = apics[i];
		if (apic.int_base <= io_apic_irq) {
			if (io_apic_irq - apic.int_base < apic.irq_count) {
				write(i, 0x10 + (io_apic_irq - apic.int_base) * 2, e & 0xFFFFFFFF);
				write(i, 0x10 + (io_apic_irq - apic.int_base) * 2 + 1, e >> 32);
				apic.used_irqs |= 1 << io_apic_irq;
				return;
			}
		}
	}

	unreachable();
}

void IoApic::register_isa_irq(u8 irq, RedirEntry entry) {
	IsaRedirInfo info = get_isa_irq_info(irq);

	u64 e = entry.vector;
	e |= as<u64>(entry.delivery_mode) << 8;
	e |= as<u64>(entry.dest_mode) << 11;
	e |= as<u64>(info.active_low) << 13;
	e |= as<u64>(info.level_triggered) << 15;
	e |= as<u64>(entry.mask) << 16;
	e |= as<u64>(entry.dest) << 56;

	write(info.io_apic_index, 0x10 + info.io_apic_entry * 2, e & 0xFFFFFFFF);
	write(info.io_apic_index, 0x10 + info.io_apic_entry * 2 + 1, e >> 32);
	apics[info.io_apic_index].used_irqs |= 1 << info.io_apic_entry;
}