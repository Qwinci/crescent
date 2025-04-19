#include "ufs.hpp"
#include "utils/driver.hpp"

static InitStatus ufs_pci_init(pci::Device& device) {
	device.enable_mem_space(true);
	device.enable_bus_master(true);

	IoSpace space {device.map_bar(0)};

	return ufs_init(space);
}

static PciDriver UFS_DRIVER {
	.init = ufs_pci_init,
	.match = PciMatch::Device,
	.devices {
		// qemu ufs
		{.vendor = 0x1B36, .device = 0x0013}
	}
};

PCI_DRIVER(UFS_DRIVER);
