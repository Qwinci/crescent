#pragma once
#include "pci.h"

enum {
	PCI_MATCH_CLASS = 1 << 0,
	PCI_MATCH_SUBCLASS = 1 << 1,
	PCI_MATCH_PROG = 1 << 2,
	PCI_MATCH_DEV = 1 << 3
};

typedef struct {
	u16 vendor;
	u16 device;
} PciDev;

typedef struct {
	u8 match;

	u16 dev_class;
	u16 dev_subclass;
	u16 dev_prog;

	void (*load)(PciHdr0* hdr);

	usize dev_count;
	PciDev devices[];
} PciDriver;

typedef enum {
	DRIVER_GENERIC,
	DRIVER_PCI
} DriverType;

typedef struct {
	DriverType type;
	union {
		PciDriver* pci_driver;
	};
} Driver;

#define PCI_DRIVER(Name, PciDev) \
__attribute__((section(".drivers"), used)) static Driver Name = { \
	.type = DRIVER_PCI, \
	.pci_driver = PciDev \
}