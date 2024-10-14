#pragma once
#include "mem/iospace.hpp"

enum class IoApicDelivery : u8 {
	Fixed = 0b0,
	LowPrio = 0b1,
	Smi = 0b10,
	Nmi = 0b100,
	Init = 0b101,
	ExtInt = 0b111
};

enum class IoApicPolarity : u8 {
	ActiveHigh = 0,
	ActiveLow = 1
};

enum class IoApicTrigger : u8 {
	Edge = 0,
	Level = 1
};

struct IoApicIrqInfo {
	IoApicDelivery delivery;
	IoApicPolarity polarity;
	IoApicTrigger trigger;
	u8 vec;
};

class IoApicManager {
public:
	void register_io_apic(u32 phys, u32 gsi_base);
	void register_override(u8 irq, u32 gsi, bool active_low, bool level_triggered);

	void register_irq(u32 gsi, IoApicIrqInfo info);
	void register_isa_irq(u32 irq, u8 vec, bool active_low = false, bool level_triggered = false);

	void deregister_irq(u32 gsi);
	void deregister_isa_irq(u32 irq);

private:
	struct IoApic {
		IoSpace space {};
		u32 gsi_base {};
		u8 gsi_count {};

		u32 read(u8 reg);
		void write(u8 reg, u32 value);
	};
	IoApic io_apics[16] {};

	struct Override {
		u32 gsi;
		bool used;
		bool active_low;
		bool level_triggered;
	};
	Override overrides[16] {};
	u8 io_apic_count {};
};

extern IoApicManager IO_APIC;
