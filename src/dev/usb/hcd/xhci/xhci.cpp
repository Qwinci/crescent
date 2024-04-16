#include "arch/irq.hpp"
#include "dev/clock.hpp"
#include "mem/iospace.hpp"
#include "utils/driver.hpp"

namespace regs {
	static constexpr BasicRegister<u8> CAPLENGTH {0x0};
	static constexpr BitRegister<u16> HCIVERSION {0x2};
	static constexpr BitRegister<u32> HCSPARAMS1 {0x4};
	static constexpr BitRegister<u32> HCSPARAMS2 {0x8};
	static constexpr BitRegister<u32> HCSPARAMS3 {0xC};
	static constexpr BitRegister<u32> HCCPARAMS1 {0x10};
	static constexpr BasicRegister<u32> DBOFF {0x14};
	static constexpr BasicRegister<u32> RTSOFF {0x18};
	static constexpr BitRegister<u32> HCCPARAMS2 {0x1C};

	static constexpr BitRegister<u32> USBCMD {0x0};
	static constexpr BitRegister<u32> USBSTS {0x4};
	static constexpr BitRegister<u32> PAGESIZE {0x8};
	static constexpr BitRegister<u32> DNCTRL {0x14};
	static constexpr BitRegister<u32> CRCR {0x18};
	static constexpr BitRegister<u64> DCBAAP {0x30};
	static constexpr BitRegister<u32> CONFIG {0x38};

	static constexpr BitRegister<u32> PORTSC {0x0};
	static constexpr BitRegister<u32> PORTPMSC {0x4};
	static constexpr BitRegister<u32> PORTLI {0x8};
	static constexpr BitRegister<u32> PORTHLPMC {0xC};

	static constexpr BitRegister<u32> MFINDEX {0x0};
	static constexpr BitRegister<u32> IMAN {0x0};
	static constexpr BitRegister<u32> IMOD {0x4};
	static constexpr BitRegister<u32> ERSTSZ {0x8};
}

namespace hcsparams1 {
	static constexpr BitField<u32, u8> MAX_SLOTS {0, 8};
	static constexpr BitField<u32, u16> MAX_INTRS {8, 11};
	static constexpr BitField<u32, u8> MAX_PORTS {24, 8};
}

namespace hccparams1 {
	static constexpr BitField<u32, bool> AC64 {0, 1};
	static constexpr BitField<u32, bool> CSZ {2, 1};
	static constexpr BitField<u32, u16> XECP {16, 16};
}

namespace usbcmd {
	static constexpr BitField<u32, bool> RS {0, 1};
	static constexpr BitField<u32, bool> HCRST {1, 1};
	static constexpr BitField<u32, bool> INTE {2, 1};
	static constexpr BitField<u32, bool> HSEE {3, 1};
}

namespace usbsts {
	static constexpr BitField<u32, bool> HCH {0, 1};
	static constexpr BitField<u32, bool> HSE {2, 1};
	static constexpr BitField<u32, bool> EINT {3, 1};
	static constexpr BitField<u32, bool> PCD {4, 1};
	static constexpr BitField<u32, bool> CNR {11, 1};
	static constexpr BitField<u32, bool> HCE {12, 1};
}

namespace crcr {
	static constexpr BitField<u64, u64> CRP {6, 58};
}

namespace config {
	static constexpr BitField<u32, u8> MAX_SLOTS_EN {0, 8};
}

namespace xcap {
	static constexpr BitField<u32, u8> ID {0, 8};
	static constexpr BitField<u32, u8> NEXT {8, 8};

	static constexpr u8 USB_LEGACY_SUPPORT = 1;
	static constexpr u8 SUPPORTED_PROTOCOL = 2;
}

namespace usblegsup {
	static constexpr BitField<u32, bool> HC_BIOS_OWNED {16, 1};
	static constexpr BitField<u32, bool> HC_OS_OWNED {24, 1};
}

struct SlotContext {
	BitValue<u32> first;
	u16 max_exit_latency {};
	u8 root_hub_port_number {};
	u8 number_of_ports {};
	BitValue<u32> second;
	BitValue<u32> third;
};

namespace slot_ctx {
	namespace first {
		static constexpr BitField<u32, u32> ROUTE_STRING {0, 20};
		static constexpr BitField<u32, bool> HUB {26, 1};
		static constexpr BitField<u32, u8> CTX_ENTRIES {27, 5};
	}

	namespace second {
		static constexpr BitField<u32, u8> PARENT_HUB_SLOT_ID {0, 8};
		static constexpr BitField<u32, u8> PARENT_PORT_NUMBER {8, 8};
		static constexpr BitField<u32, u8> TT_THINK_TIME {16, 2};
		static constexpr BitField<u32, u8> INTERRUPTER_TARGET {22, 10};
	}

	namespace third {
		static constexpr BitField<u32, u8> USB_DEVICE_ADDRESS {0, 8};
		static constexpr BitField<u32, u8> SLOT_STATE {27, 5};
	}
}

struct [[gnu::packed]] EndpointContext {
	u32 first;
	u32 second;
	u64 tr_dequeue_ptr;
	u32 third;
};

namespace endpoint_ctx {
	namespace first {
		static constexpr BitField<u32, u8> EP_STATE {0, 3};
		static constexpr BitField<u32, u8> MULT {8, 2};
		static constexpr BitField<u32, u8> MAX_P_STREAMS {10, 5};
		static constexpr BitField<u32, bool> LSA {15, 1};
		static constexpr BitField<u32, u8> INTERVAL {16, 8};
		static constexpr BitField<u32, u8> MAX_ESIT_PAYLOAD_HI {24, 8};
	}

	namespace second {
		static constexpr BitField<u32, u8> ERROR_COUNT {1, 2};
		static constexpr BitField<u32, u8> EP_TYPE {3, 3};
		static constexpr BitField<u32, bool> HID {7, 1};
		static constexpr BitField<u32, u8> MAX_BURST_SIZE {8, 8};
		static constexpr BitField<u32, u16> MAX_PACKET_SIZE {16, 16};
	}

	namespace tr_dequeue_ptr {
		static constexpr u64 DCS = 1;
	}

	namespace third {
		static constexpr BitField<u32, u16> AVG_TRB_LEN {0, 16};
		static constexpr BitField<u32, u16> MAX_ESIT_PAYLOAD_LO {16, 16};
	}
}

namespace iman {
	static constexpr BitField<u32, bool> IP {0, 1};
	static constexpr BitField<u32, bool> IE {1, 1};
}

struct XhciController {
	explicit XhciController(pci::Device& device) : device {device} {
		cap_space = IoSpace {device.map_bar(0)};
		device.enable_io_space(true);
		device.enable_mem_space(true);
		device.enable_bus_master(true);
		device.enable_legacy_irq(true);
		op_space = cap_space.subspace(cap_space.load(regs::CAPLENGTH));
		runtime_space = cap_space.subspace(cap_space.load(regs::RTSOFF));
	}

	bool init() {
		auto hcc_params1 = cap_space.load(regs::HCCPARAMS1);
		assert(hcc_params1 & hccparams1::AC64);
		u32 extended_cap_offset = (hcc_params1 & hccparams1::XECP) << 2;
		while (true) {
			BitValue<u32> value {cap_space.load<u32>(extended_cap_offset)};
			auto id = value & xcap::ID;
			if (id == xcap::USB_LEGACY_SUPPORT) {
				println("[kernel][xhci]: found legacy support cap");
				value |= usblegsup::HC_OS_OWNED(true);
				cap_space.store<u32>(extended_cap_offset, value);
				auto res = with_timeout([&]() {
					BitValue<u32> leg_sup {cap_space.load<u32>(extended_cap_offset)};
					return !(leg_sup & usblegsup::HC_BIOS_OWNED);
				}, US_IN_S);
				if (!res) {
					println("[kernel][xhci]: failed to acquire ownership from the bios");
					return false;
				}
			}
			u32 next_offset = (value & xcap::NEXT) << 2;
			if (!next_offset) {
				break;
			}
			extended_cap_offset += next_offset;
		}

		auto cmd = op_space.load(regs::USBCMD);
		cmd &= ~usbcmd::RS;
		op_space.store(regs::USBCMD, cmd);

		while (!(op_space.load(regs::USBSTS) & usbsts::HCH));

		cmd = op_space.load(regs::USBCMD);
		cmd |= usbcmd::HCRST(true);
		op_space.store(regs::USBCMD, cmd);

		while (op_space.load(regs::USBCMD) & usbcmd::HCRST);
		while (op_space.load(regs::USBSTS) & usbsts::CNR);

		auto hcsparams1 = cap_space.load(regs::HCSPARAMS1);
		auto slots = hcsparams1 & hcsparams1::MAX_SLOTS;
		println("[kernel][xhci]: ", slots, " slots");

		auto config = op_space.load(regs::CONFIG);
		config &= ~config::MAX_SLOTS_EN;
		config |= config::MAX_SLOTS_EN(slots);
		op_space.store(regs::CONFIG, config);

		usize dcbaab_phys = pmalloc(1);
		assert(dcbaab_phys);
		op_space.store(regs::DCBAAP, dcbaab_phys);

		auto first_trb_phys = pmalloc(1);
		assert(first_trb_phys);

		auto crcr = op_space.load(regs::CRCR);
		crcr &= ~crcr::CRP;
		crcr |= crcr::CRP(first_trb_phys);
		op_space.store(regs::CRCR, crcr);

		cmd = op_space.load(regs::USBCMD);
		cmd |= usbcmd::RS(true);
		op_space.store(regs::USBCMD, cmd);

		while (op_space.load(regs::USBSTS) & usbsts::HCH);

		return true;
	}

	pci::Device& device;
	IoSpace cap_space;
	IoSpace op_space;
	IoSpace runtime_space;
	IrqHandler irq_handler {
		.fn = [](IrqFrame*) {
			println("xhci irq");
			return true;
		},
		.can_be_shared = false
	};
};

static bool xhci_init(pci::Device& device) {
	println("[kernel]: xhci initializing on ", zero_pad(4), Fmt::Hex,
			device.hdr0->common.vendor_id, ":", device.hdr0->common.device_id,
			Fmt::Reset);

	auto controller = new XhciController(device);
	if (!controller->init()) {
		delete controller;
	}

	return true;
}

static bool xhci_match(pci::Device& device) {
	auto prog = device.hdr0->common.prog_if;
	return prog >= 0x30 && prog < 0x40;
}

static PciDriver XHCI_DRIVER {
	.init = xhci_init,
	.match = PciMatch::Class | PciMatch::Subclass,
	._class = 0xC,
	.subclass = 0x3,
	.fine_match = xhci_match
};

//PCI_DRIVER(XHCI_DRIVER);
