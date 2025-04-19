#include "arch/irq.hpp"
#include "dev/clock.hpp"
#include "mem/mem.hpp"
#include "utils/driver.hpp"
#include "xhci.hpp"

namespace {
	void enable_irqs(void* arg, bool enable) {
		auto node = static_cast<DtbNode*>(arg);

		auto irq = node->irqs[0];
		GIC->mask_irq(irq.num, !enable);
	}

	bool install_irq(void* arg, IrqHandler* handler) {
		auto node = static_cast<DtbNode*>(arg);

		assert(node->irqs.size() >= 1);

		auto irq = node->irqs[0];

		GIC->set_mode(irq.num, irq.mode);
		register_irq_handler(irq.num, handler);
		return true;
	}

	void uninstall_irq(void* arg, IrqHandler* handler) {
		auto node = static_cast<DtbNode*>(arg);

		auto irq = node->irqs[0];
		GIC->mask_irq(irq.num, true);
		deregister_irq_handler(irq.num, handler);
	}
}

namespace regs {
	static constexpr BitRegister<u32> GCTL {0xC110};
	static constexpr BitRegister<u32> GUSB3PIPECTL0 {0xC2C0};
}

namespace gctl {
	static constexpr BitField<u32, u8> PRTCAPDIR {12, 3};
}

namespace prtcapdir {
	static constexpr u8 HOST = 1;
	static constexpr u8 DEVICE = 2;
	static constexpr u8 OTG = 3;
}

namespace gusb3pipectl {
	static constexpr BitField<u32, bool> SUSPHY {17, 1};
}

static InitStatus xhci_dwc3_init(DtbNode& node) {
	println("[kernel]: xhci dwc3 init");

	auto phys = node.prop("usb-phy");
	if (phys) {
		u32 first_phy = phys->read<u32>(0);
		auto phy_node = *DTB->handle_map.get(first_phy);
		if (!phy_node->data) {
			return InitStatus::Defer;
		}
	}

	assert(!node.regs.is_empty());
	IoSpace space {node.regs[0].addr, node.regs[0].size};
	auto status = space.map(CacheMode::Uncached);
	assert(status);

	auto gctl = space.load(regs::GCTL);
	gctl &= ~gctl::PRTCAPDIR;
	gctl |= gctl::PRTCAPDIR(prtcapdir::OTG);
	space.store(regs::GCTL, gctl);

	auto gusb3pipectl = space.load(regs::GUSB3PIPECTL0);
	gusb3pipectl |= gusb3pipectl::SUSPHY(true);
	space.store(regs::GUSB3PIPECTL0, gusb3pipectl);

	xhci::GenericOps ops {
		.enable_irqs = enable_irqs,
		.install_irq = install_irq,
		.uninstall_irq = uninstall_irq,
		.arg = &node
	};

	return xhci::init(ops, space);
}

static DtDriver DWC3_DRIVER {
	.init = xhci_dwc3_init,
	.compatible {
		"snps,dwc3"
	},
	.depends {
		"gic",
		"timer"
	}
};

DT_DRIVER(DWC3_DRIVER);
