#include "drivers/dev.hpp"
#include "console.hpp"

void init_intel_graphics(Pci::Header0* hdr) {
	println("initializing intel graphics");
}

static PciDriver pci_driver {
	.match = PciDriver::MATCH_DEV,
	.load = init_intel_graphics,
	.dev_count = 2,
	.devices {
		{.vendor = 0x8086, .device = 0x9A40},
		{.vendor = 0x8086, .device = 0x9A49}
	}
};

PCI_DRIVER(intel_tigerlake_igpu, pci_driver);