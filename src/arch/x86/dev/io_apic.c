#include "io_apic.h"
#include "assert.h"

typedef struct {
	u32 global_base;
	bool enabled;
	bool active_low;
	bool level_triggered;
} IoApicOverride;
IoApicOverride overrides[16];

void io_apic_register_override(u8 irq, u32 global_base, bool active_low, bool level_triggered) {
	overrides[irq] = (IoApicOverride) {
		.global_base = global_base,
		.enabled = true,
		.active_low = active_low,
		.level_triggered = level_triggered
	};
}

u32 io_apic_read(IoApic* self, u8 reg) {
	*(volatile u32*) self->base = reg;
	return *(volatile u32*) (self->base + 0x10);
}

static void io_apic_write(IoApic* self, u8 reg, u32 value) {
	*(volatile u32*) self->base = reg;
	*(volatile u32*) (self->base + 0x10) = value;
}

typedef struct {
	usize io_apic_index;
	u8 io_apic_entry;
	bool active_low;
	bool level_triggered;
} IsaRedirEntry;

static IsaRedirEntry get_isa_irq_info(u8 irq) {
	IoApicOverride override = overrides[irq];
	if (override.enabled) {
		for (usize i = 0; i < io_apic_count; ++i) {
			const IoApic* apic = &io_apics[i];
			if (apic->int_base <= override.global_base &&
				override.global_base - apic->int_base < apic->irq_count) {
				return (IsaRedirEntry) {
					.io_apic_index = i,
					.io_apic_entry = (u8) (override.global_base - apic->int_base),
					.active_low = override.active_low,
					.level_triggered = override.level_triggered
				};
			}
		}
	}

	for (usize i = 0; i < io_apic_count; ++i) {
		const IoApic* apic = &io_apics[i];
		if (apic->int_base == 0) {
			return (IsaRedirEntry) {
				.io_apic_index = i,
				.io_apic_entry = irq,
				.active_low = false,
				.level_triggered = false
			};
		}
	}

	assert(false && "entered unreachable code");
}

void io_apic_register_isa_irq(u8 irq, IoApicRedirEntry entry) {
	IsaRedirEntry info = get_isa_irq_info(irq);

	u64 e = entry.vec;
	e |= (u64) entry.delivery_mode << 8;
	e |= (u64) entry.dest_mode << 11;
	e |= (u64) info.active_low << 13;
	e |= (u64) info.level_triggered << 15;
	e |= (u64) entry.mask << 16;
	e |= (u64) entry.dest << 56;

	io_apic_write(&io_apics[info.io_apic_index], 0x10 + info.io_apic_entry * 2, e);
	io_apic_write(&io_apics[info.io_apic_index], 0x10 + info.io_apic_entry * 2 + 1, e >> 32);
	io_apics[info.io_apic_index].used_irqs |= 1ULL << info.io_apic_entry;
}

void io_apic_register_irq(u32 io_apic_irq, IoApicRedirEntry entry) {
	u64 e = entry.vec;
	e |= (u64) entry.delivery_mode << 8;
	e |= (u64) entry.dest_mode << 11;
	e |= (u64) entry.pin << 13;
	e |= (u64) entry.trigger << 15;
	e |= (u64) entry.mask << 16;
	e |= (u64) entry.dest << 56;

	for (usize i = 0; i < io_apic_count; ++i) {
		IoApic* apic = &io_apics[i];
		if (apic->int_base <= io_apic_irq && io_apic_irq - apic->int_base < apic->irq_count) {
			io_apic_write(apic, 0x10 + (io_apic_irq - apic->int_base) * 2, e);
			io_apic_write(apic, 0x10 + (io_apic_irq - apic->int_base) * 2 + 1, e >> 32);
			apic->used_irqs |= 1ULL << io_apic_irq;
			return;
		}
	}
}

bool io_apic_is_entry_free(u32 io_apic_irq) {
	for (usize i = 0; i < io_apic_count; ++i) {
		const IoApic* apic = &io_apics[i];
		if (apic->int_base <= io_apic_irq && io_apic_irq - apic->int_base < apic->irq_count) {
			return apic->used_irqs & 1ULL << (io_apic_irq - apic->int_base);
		}
	}
	return false;
}

u32 io_apic_get_free_irq() {
	for (usize i = 0; i < io_apic_count; ++i) {
		IoApic* apic = &io_apics[i];
		for (u8 j = 0; j < apic->irq_count; ++j) {
			if (!(apic->used_irqs & 1ULL << j)) {
				apic->used_irqs |= 1ULL << j;
				return apic->int_base + j;
			}
		}
	}
	return 0;
}

usize io_apic_count = 0;
IoApic io_apics[MAX_IO_APIC_COUNT];