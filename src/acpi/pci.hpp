#pragma once
#include "types.hpp"
#include "optional.hpp"

namespace acpi {
	struct LegacyPciIrq {
		u32 gsi;
		bool edge_triggered;
		bool active_high;
	};

	void pci_init();
	kstd::optional<LegacyPciIrq> get_legacy_pci_irq(u16 seg, u8 bus, u8 device, u8 pci_pin);
}
