#include "pci.hpp"
#include "vm.hpp"

std::vector<std::unique_ptr<PciDevice>> PCI_DEVICES;

void pci_add_device(std::unique_ptr<PciDevice> device) {
	PCI_DEVICES.push_back(std::move(device));
}

uint32_t pci_config_read(PciAddress addr, uint16_t offset, uint8_t size) {
	for (auto& dev : PCI_DEVICES) {
		if (dev->addr == addr) {
			uint32_t value = 0;
			memcpy(&value, dev->config.ptr.get() + offset, size);
			return value;
		}
	}

	return 0xFFFFFFFF;
}

void pci_config_write(PciAddress addr, uint16_t offset, uint32_t value, uint8_t size) {
	for (auto& dev : PCI_DEVICES) {
		if (dev->addr == addr) {
			if (offset >= 0x10 && offset <= 0x24) {
				uint8_t bar = (offset - 0x10) / 4;

				if (value == 0xFFFFFFFF) {
					dev->config.set_bar(bar, -dev->config.bar_sizes[bar]);
					return;
				}

				if (dev->config.bars[bar]) {
					if (dev->config.bar_allocated[bar] != (value & ~0b111)) {
						if (dev->config.bar_allocated[bar]) {
							VM->unmap_mem(dev->config.bar_allocated[bar], dev->config.bar_sizes[bar]);
						}

						VM->map_mem(value & ~0b111, dev->config.bars[bar], dev->config.bar_sizes[bar]);
						dev->config.bar_allocated[bar] = value & ~0b111;
					}
				}
			}
			else if (offset == 0x30) {
				if (value == ~0x7FFU || value == ~0U || value == ~1U) {
					dev->config.set_expansion_rom(-dev->config.bar_sizes[6]);
					return;
				}

				if (dev->config.bars[6]) {
					if (dev->config.bar_allocated[6] != (value & ~1)) {
						if (dev->config.bar_allocated[6]) {
							VM->unmap_mem(dev->config.bar_allocated[6], dev->config.bar_sizes[6]);
						}

						VM->map_mem(value & ~1, dev->config.bars[6], dev->config.bar_sizes[6]);
						dev->config.bar_allocated[6] = value & ~1;
					}
				}
			}

			memcpy(dev->config.ptr.get() + offset, &value, size);
			break;
		}
	}
}
