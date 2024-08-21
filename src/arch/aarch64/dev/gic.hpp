#pragma once
#include "types.hpp"

enum class TriggerMode {
	None,
	Edge,
	Level
};

struct Gic {
	virtual ~Gic() = default;

	virtual void mask_irq(u32 irq, bool mask) = 0;
	virtual void set_mode(u32 irq, TriggerMode mode) = 0;
	virtual void set_priority(u32 irq, u8 priority) = 0;
	virtual void set_affinity(u32 irq, u32 affinity) = 0;

	virtual u32 ack() = 0;
	virtual void eoi(u32 irq) = 0;

	virtual void send_ipi(u32 affinity, u8 id) = 0;
	virtual void send_ipi_to_others(u8 id) = 0;

	u32 num_irqs {};
};

void gic_init_on_cpu();

extern Gic* GIC;
extern u32 GIC_VERSION;
