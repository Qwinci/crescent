#pragma once
#include "types.hpp"

enum class IoApicDeliveryMode : u8 {
	Fixed = 0,
	LowPriority = 1,
	Smi = 0b10,
	Nmi = 0b100,
	Init = 0b101,
	ExtInt = 0b111
};

enum class IoApicDestination : u8 {
	Physical = 0,
	Logical = 1
};

enum class IoApicPinPolarity : u8 {
	ActiveHigh = 0,
	ActiveLow = 1
};

enum class IoApicTriggerMode : u8 {
	Edge = 0,
	Level = 1
};

struct IoApic {
	static IoApic apics[16];
	static u8 io_apic_count;

	struct RedirEntry {
		u8 vector;
		IoApicDeliveryMode delivery_mode;
		IoApicDestination dest_mode;
		IoApicPinPolarity polarity;
		IoApicTriggerMode trigger_mode;
		u8 mask;
		u8 dest;
	};

	static void register_isa_irq(u8 irq, RedirEntry entry);
	static void register_irq(u8 io_apic_irq, RedirEntry entry);
	static bool is_entry_free(u8 io_apic_irq);
	static i16 get_free_irq();
private:
	struct IsaRedirInfo {
		u8 io_apic_index;
		u8 io_apic_entry;
		bool active_low;
		bool level_triggered;
	};

	static IsaRedirInfo get_isa_irq_info(u8 irq);
	static void write(u8 apic, u8 reg, u32 value);
	[[nodiscard]] static u32 read(u8 apic, u8 reg);

	static void register_override(u8 irq, u8 global_base, bool active_low, bool level_triggered);

	struct Override {
		bool enabled;
		u8 global_base;
		bool active_low;
		bool level_triggered;
	};

	static Override overrides[16];

	friend void parse_madt(const void* madt_ptr);

	usize base;
	u32 int_base;
	u8 irq_count;
	u32 used_irqs;
};