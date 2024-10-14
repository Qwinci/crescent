#include "xhci.hpp"
#include "utils/driver.hpp"
#include "arch/irq.hpp"

namespace {
	void enable_irqs(void* arg, bool enable) {
		auto device = static_cast<pci::Device*>(arg);
		device->enable_irqs(enable);
	}

	bool install_irq(void* arg, IrqHandler* handler) {
		auto device = static_cast<pci::Device*>(arg);
		assert(device->alloc_irqs(1, 1, pci::IrqFlags::All));
		u32 irq = device->get_irq(0);
		register_irq_handler(irq, handler);
		return true;
	}

	void uninstall_irq(void* arg, IrqHandler* handler) {
		auto device = static_cast<pci::Device*>(arg);
		deregister_irq_handler(device->get_irq(0), handler);
		device->free_irqs();
	}
}

static InitStatus xhci_pci_init(pci::Device& device) {
	println("[kernel]: xhci pci init");

	device.enable_io_space(true);
	device.enable_mem_space(true);
	device.enable_bus_master(true);

	IoSpace space {device.map_bar(0)};

	xhci::GenericOps ops {
		.enable_irqs = enable_irqs,
		.install_irq = install_irq,
		.uninstall_irq = uninstall_irq,
		.arg = &device
	};

	return xhci::init(ops, space);
}

static bool xhci_match(pci::Device& device) {
	auto prog = device.read(pci::common::PROG_IF);
	return prog >= 0x30 && prog < 0x40;
}

static PciDriver XHCI_DRIVER {
	.init = xhci_pci_init,
	.match = PciMatch::Class | PciMatch::Subclass,
	._class = 0xC,
	.subclass = 0x3,
	.fine_match = xhci_match
};

PCI_DRIVER(XHCI_DRIVER);
