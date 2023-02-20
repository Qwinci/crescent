#pragma once
#include "pci.hpp"
#include "types.hpp"

struct PciDriver {
	u8 match;

	enum {
		MATCH_CLASS = 1 << 0,
		MATCH_SUBCLASS = 1 << 1,
		MATCH_PROG = 1 << 2,
		MATCH_DEV = 1 << 3
	};

	u16 dev_class;
	u16 dev_subclass;
	u16 dev_prog;

	void (*load)(Pci::Header0* hdr);
	void (*unload)();

	usize dev_count;
	struct Dev {
		u16 vendor;
		u16 device;
	} devices[];
};

struct Driver {
	enum {
		PCI,
		GENERIC
	} type;
	union {
		PciDriver* pci_dev;
	};
};

#define PCI_DRIVER(Name, PciDev) \
[[gnu::section(".drivers"), gnu::used]] static Driver Name { \
	.type = Driver::PCI, \
	.pci_dev = &PciDev \
}

extern Driver DRIVERS_START[];
extern Driver DRIVERS_END[];