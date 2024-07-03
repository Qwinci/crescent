#include "arch/irq.hpp"
#include "dev/net/dhcp.hpp"
#include "dev/net/ethernet.hpp"
#include "mem/iospace.hpp"
#include "mem/mem.hpp"
#include "mem/vspace.hpp"
#include "nic.hpp"
#include "sched/process.hpp"
#include "utils/driver.hpp"

namespace regs {
	static constexpr BasicRegister<u8> IDR0 {0x0};
	static constexpr BasicRegister<u8> IDR1 {0x1};
	static constexpr BasicRegister<u8> IDR2 {0x2};
	static constexpr BasicRegister<u8> IDR3 {0x3};
	static constexpr BasicRegister<u8> IDR4 {0x4};
	static constexpr BasicRegister<u8> IDR5 {0x5};
	static constexpr BasicRegister<u32> TNPDS_LOW {0x20};
	static constexpr BasicRegister<u32> TNPDS_HIGH {0x24};
	static constexpr BitRegister<u8> CMD {0x37};
	static constexpr BitRegister<u8> TPPOLL {0x38};
	static constexpr BitRegister<u16> IMR {0x3C};
	static constexpr BitRegister<u16> ISR {0x3E};
	static constexpr BitRegister<u32> TR_CFG {0x40};
	static constexpr BitRegister<u32> RX_CFG {0x44};
	static constexpr BitRegister<u8> CMD_9346 {0x50};
	static constexpr BitRegister<u32> PHYAR {0x60};
	static constexpr BitRegister<u8> PHY_STS {0x6C};
	static constexpr BitRegister<u16> RMS {0xDA};
	static constexpr BitRegister<u16> CPCR {0xE0};
	static constexpr BasicRegister<u32> RDSAR_LOW {0xE4};
	static constexpr BasicRegister<u32> RDSAR_HIGH {0xE8};
	static constexpr BitRegister<u8> MTPS {0xEC};
}

namespace regs_8139 {
	static constexpr BitRegister<u8> TPPOLL {0xD9};
}

namespace cmd {
	static constexpr BitField<u8, bool> TE {2, 1};
	static constexpr BitField<u8, bool> RE {3, 1};
	static constexpr BitField<u8, bool> RST {4, 1};
}

namespace tppoll {
	static constexpr BitField<u8, bool> NPQ {6, 1};
}

namespace imr_isr {
	static constexpr BitField<u16, bool> ROK {0, 1};
	static constexpr BitField<u16, bool> RER {1, 1};
	static constexpr BitField<u16, bool> TOK {2, 1};
	static constexpr BitField<u16, bool> TER {3, 1};
	static constexpr BitField<u16, bool> RDU {4, 1};
	static constexpr BitField<u16, bool> LINK_CHG {5, 1};
	static constexpr BitField<u16, bool> FOVW {6, 1};
	static constexpr BitField<u16, bool> TDU {7, 1};
	static constexpr BitField<u16, bool> SW_INT {8, 1};
	static constexpr BitField<u16, bool> FIFO_EMPTY {9, 1};
	static constexpr BitField<u16, bool> TIME_OUT {14, 1};
}

namespace tr_config {
	static constexpr BitField<u32, u8> MX_DMA {8, 3};
	static constexpr BitField<u32, bool> NO_CRC {16, 1};
	static constexpr BitField<u32, u8> IFG2 {19, 1};
	static constexpr BitField<u32, u8> IFG1_0 {24, 2};
}

namespace rx_config {
	static constexpr BitField<u32, bool> AAP {0, 1};
	static constexpr BitField<u32, bool> APM {1, 1};
	static constexpr BitField<u32, bool> AM {2, 1};
	static constexpr BitField<u32, bool> AB {3, 1};
	static constexpr BitField<u32, bool> AR {4, 1};
	static constexpr BitField<u32, bool> AER {5, 1};
	static constexpr BitField<u32, u8> MX_DMA {8, 3};
	static constexpr BitField<u32, u8> RX_FTH {13, 3};
}

namespace cmd_9346 {
	static constexpr BitField<u8, u8> EEM {6, 2};

	static constexpr u8 EEM_NORMAL = 0b00;
	static constexpr u8 EEM_CONFIG_WRITE = 0b11;
}

namespace phyar {
	static constexpr BitField<u32, u16> DATA {0, 16};
	static constexpr BitField<u32, u8> REG_ADDR {16, 5};
	static constexpr BitField<u32, bool> FLAG {31, 1};
}

namespace phy_sts {
	static constexpr BitField<u8, bool> LINK_STS {1, 1};
}

namespace rms {
	static constexpr BitField<u16, u16> RMS {0, 14};
}

namespace cpcr {
	static constexpr BitField<u16, bool> TX {0, 1};
	static constexpr BitField<u16, bool> RX {1, 1};
	static constexpr BitField<u16, bool> RX_CHK_SUM {5, 1};
	static constexpr BitField<u16, bool> RX_VLAN {6, 1};
}

namespace mtps {
	static constexpr BitField<u8, u8> MTPS {0, 6};
}

namespace phy_regs {
	static constexpr u8 BMCR = 0;
	static constexpr u8 BMSR = 1;
}

namespace bmcr {
	static constexpr BitField<u16, bool> RESTART_AN {9, 1};
	static constexpr BitField<u16, bool> ISOLATE {10, 1};
	static constexpr BitField<u16, bool> PWD {11, 1};
	static constexpr BitField<u16, bool> ANE {12, 1};
	static constexpr BitField<u16, bool> LOOPBACK {14, 1};
	static constexpr BitField<u16, bool> RESET {15, 1};
}

namespace bmsr {
	static constexpr BitField<u16, bool> AUTO_NEG_ABILITY {3, 1};
	static constexpr BitField<u16, bool> AUTO_NEG_COMPLETE {5, 1};
}

namespace tx_flags {
	static constexpr BitField<u16, bool> TCPCS {0, 1};
	static constexpr BitField<u16, bool> UDPCS {1, 1};
	static constexpr BitField<u16, bool> IPCS {2, 1};
	static constexpr BitField<u16, bool> LGSEN {11, 1};
	static constexpr BitField<u16, bool> LS {12, 1};
	static constexpr BitField<u16, bool> FS {13, 1};
	static constexpr BitField<u16, bool> EOR {14, 1};
	static constexpr BitField<u16, bool> OWN {15, 1};
}

struct TxDescriptor {
	u16 frame_len {};
	BitValue<u16> flags {};
	u32 vlan {};
	u64 buffer {};
};

namespace rx_empty_flags {
	static constexpr BitField<u32, u16> BUFFER_SIZE {0, 14};
	static constexpr BitField<u32, bool> EOR {30, 1};
	static constexpr BitField<u32, bool> OWN {31, 1};
};

namespace rx_full_flags {
	static constexpr BitField<u32, u16> FRAME_LEN {0, 14};
	static constexpr BitField<u32, bool> TCPF {14, 1};
	static constexpr BitField<u32, bool> UDPF {15, 1};
	static constexpr BitField<u32, bool> IPF {16, 1};
	static constexpr BitField<u32, u8> PID {17, 2};
	static constexpr BitField<u32, bool> CRC {19, 1};
	static constexpr BitField<u32, bool> RUNT {20, 1};
	static constexpr BitField<u32, bool> RES {21, 1};
	static constexpr BitField<u32, bool> RWT {22, 1};
	static constexpr BitField<u32, bool> BAR {25, 1};
	static constexpr BitField<u32, bool> PAM {26, 1};
	static constexpr BitField<u32, bool> MAR {27, 1};
	static constexpr BitField<u32, bool> LS {28, 1};
	static constexpr BitField<u32, bool> FS {29, 1};
	static constexpr BitField<u32, bool> EOR {30, 1};
	static constexpr BitField<u32, bool> OWN {31, 1};
}

struct RxDescriptor {
	BitValue<u32> flags {};
	u32 vlan {};
	u64 buffer {};
};

struct Rtl : public Nic {
	explicit Rtl(pci::Device& device) : device {device} {
		bool bar_found = false;
		for (u32 i = 0; i < 6; ++i) {
			if (device.is_io_space(i)) {
				continue;
			}
			auto size = device.get_bar_size(i);
			if (!size) {
				if (device.is_64bit(i)) {
					++i;
				}
				continue;
			}

			auto bar = device.map_bar(i);
			space = IoSpace {bar};
			bar_found = true;
			break;
		}

		assert(bar_found);

		device.enable_mem_space(true);
		device.enable_io_space(true);
		device.enable_bus_master(true);
		assert(device.alloc_irqs(1, 1, pci::IrqFlags::All));
		auto irq_num = device.get_irq(0);
		register_irq_handler(irq_num, &irq_handler);

		mac.data[0] = space.load(regs::IDR0);
		mac.data[1] = space.load(regs::IDR1);
		mac.data[2] = space.load(regs::IDR2);
		mac.data[3] = space.load(regs::IDR3);
		mac.data[4] = space.load(regs::IDR4);
		mac.data[5] = space.load(regs::IDR5);

		println("[kernel][nic]: rtl mac is ", zero_pad(2), Fmt::Hex,
				mac.data[0], ":",
				mac.data[1], ":",
				mac.data[2], ":",
				mac.data[3], ":",
				mac.data[4], ":",
				mac.data[5],
				Fmt::Reset, Pad {});
	}

	BitValue<u16> phy_read(u8 reg) {
		auto phyar = space.load(regs::PHYAR);
		phyar &= ~phyar::FLAG;
		phyar &= ~phyar::REG_ADDR;
		phyar &= ~phyar::DATA;
		phyar |= phyar::REG_ADDR(reg);
		space.store(regs::PHYAR, phyar);

		while (!((phyar = space.load(regs::PHYAR)) & phyar::FLAG));
		return {phyar & phyar::DATA};
	}

	void phy_write(u8 reg, u16 value) {
		auto phyar = space.load(regs::PHYAR);
		phyar |= phyar::FLAG(true);
		phyar &= ~phyar::REG_ADDR;
		phyar &= ~phyar::DATA;
		phyar |= phyar::REG_ADDR(reg);
		phyar |= phyar::DATA(value);
		space.store(regs::PHYAR, phyar);

		while (space.load(regs::PHYAR) & phyar::FLAG);
	}

	void init() {
		auto cpcr = space.load(regs::CPCR);
		cpcr &= ~cpcr::RX_CHK_SUM;
		cpcr &= ~cpcr::RX_VLAN;

		if (device.hdr0->common.device_id == 0x8139) {
			is_8139 = true;
			cpcr |= cpcr::TX(true);
			cpcr |= cpcr::RX(true);

			desc_count = kstd::min(u32 {PAGE_SIZE / sizeof(TxDescriptor)}, u32 {64});
		}
		else {
			desc_count = PAGE_SIZE / sizeof(TxDescriptor);
		}

		space.store(regs::CPCR, cpcr);

		auto cmd = space.load(regs::CMD);
		cmd &= ~cmd::RE;
		cmd &= ~cmd::TE;
		cmd |= cmd::RST(true);
		space.store(regs::CMD, cmd);

		while (space.load(regs::CMD) & cmd::RST);

		if (is_8139) {
			cpcr |= cpcr::TX(true);
			cpcr |= cpcr::RX(true);
			space.store(regs::CPCR, cpcr);
		}

		auto imr = space.load(regs::IMR);
		imr &= ~imr_isr::TIME_OUT;
		imr &= ~imr_isr::FIFO_EMPTY;
		imr |= imr_isr::SW_INT(true);
		imr &= ~imr_isr::TDU;
		imr &= ~imr_isr::FOVW;
		imr |= imr_isr::LINK_CHG(true);
		imr &= ~imr_isr::RDU;
		imr |= imr_isr::TER(true);
		imr |= imr_isr::TOK(true);
		imr |= imr_isr::RER(true);
		imr |= imr_isr::ROK(true);
		space.store(regs::IMR, imr);

		auto rx_config = space.load(regs::RX_CFG);
		rx_config &= ~rx_config::RX_FTH;
		rx_config |= rx_config::RX_FTH(0b111);
		rx_config &= ~rx_config::MX_DMA;
		rx_config |= rx_config::MX_DMA(0b111);
		rx_config &= ~rx_config::AER;
		rx_config &= ~rx_config::AR;
		rx_config |= rx_config::AB(true);
		rx_config |= rx_config::AM(true);
		rx_config |= rx_config::APM(true);
		rx_config &= ~rx_config::AAP;
		space.store(regs::RX_CFG, rx_config);

		if (!is_8139) {
			auto rms = space.load(regs::RMS);
			rms &= ~rms::RMS;
			rms |= rms::RMS(8191);
			space.store(regs::RMS, rms);
		}

		auto mtps = space.load(regs::MTPS);
		mtps &= ~mtps::MTPS;
		mtps |= mtps::MTPS(0x3B);
		space.store(regs::MTPS, mtps);

		auto rx_desc_phys = pmalloc(1);
		auto tx_desc_phys = pmalloc(1);
		assert(rx_desc_phys);
		assert(tx_desc_phys);
		space.store(regs::RDSAR_HIGH, rx_desc_phys >> 32);
		space.store(regs::RDSAR_LOW, rx_desc_phys);
		space.store(regs::TNPDS_HIGH, tx_desc_phys >> 32);
		space.store(regs::TNPDS_LOW, tx_desc_phys);

		auto* rx_desc_virt = KERNEL_VSPACE.alloc(PAGE_SIZE);
		auto* tx_desc_virt = KERNEL_VSPACE.alloc(PAGE_SIZE);
		assert(rx_desc_virt);
		assert(tx_desc_virt);
		assert(KERNEL_PROCESS->page_map.map(
			reinterpret_cast<u64>(rx_desc_virt),
			rx_desc_phys,
			PageFlags::Read | PageFlags::Write,
			CacheMode::Uncached));
		assert(KERNEL_PROCESS->page_map.map(
			reinterpret_cast<u64>(tx_desc_virt),
			tx_desc_phys,
			PageFlags::Read | PageFlags::Write,
			CacheMode::Uncached));

		tx_desc = static_cast<TxDescriptor*>(tx_desc_virt);
		rx_desc = static_cast<RxDescriptor*>(rx_desc_virt);
		memset(tx_desc_virt, 0, PAGE_SIZE);
		memset(rx_desc_virt, 0, PAGE_SIZE);

		for (usize i = 0; i < desc_count; ++i) {
			auto tx_buffer_phys = pmalloc(1);
			auto rx_buffer_phys = pmalloc(1);
			assert(tx_buffer_phys);
			assert(rx_buffer_phys);

			rx_desc[i].flags |= rx_empty_flags::OWN(true) | rx_empty_flags::BUFFER_SIZE(PAGE_SIZE);

			if (i == desc_count - 1) {
				tx_desc[i].flags |= tx_flags::EOR(true);
				rx_desc[i].flags |= rx_empty_flags::EOR(true);
			}

			tx_desc[i].buffer = tx_buffer_phys;
			rx_desc[i].buffer = rx_buffer_phys;
		}

		cmd = space.load(regs::CMD);
		cmd |= cmd::TE(true);
		space.store(regs::CMD, cmd);

		auto tr_config = space.load(regs::TR_CFG);
		tr_config &= ~tr_config::IFG1_0;
		tr_config &= ~tr_config::IFG2;
		tr_config |= tr_config::IFG1_0(0b11);
		tr_config &= ~tr_config::NO_CRC;
		tr_config &= ~tr_config::MX_DMA;
		tr_config |= tr_config::MX_DMA(0b111);
		space.store(regs::TR_CFG, tr_config);

		cmd = space.load(regs::CMD);
		cmd |= cmd::TE(true);
		cmd |= cmd::RE(true);
		space.store(regs::CMD, cmd);

		device.enable_irqs(true);
	}

	void send(const void* data, u32 size) override {
		assert(size <= PAGE_SIZE);

		auto& desc = tx_desc[tx_desc_ptr];
		if (desc.flags & tx_flags::OWN) {
			println("[kernel][nic]: rtl send buffer overflow");
			return;
		}

		desc.flags = {tx_flags::OWN(true) | tx_flags::FS(true) | tx_flags::LS(true)};
		if (tx_desc_ptr == desc_count - 1) {
			desc.flags |= tx_flags::EOR(true);
		}
		desc.vlan = 0;
		desc.frame_len = size;
		memcpy(to_virt<void>(desc.buffer), data, size);

		tx_desc_ptr = (tx_desc_ptr + 1) % desc_count;

		if (is_8139) {
			space.store(regs_8139::TPPOLL, tppoll::NPQ(true));
		}
		else {
			space.store(regs::TPPOLL, tppoll::NPQ(true));
		}
	}

	bool on_irq() {
		auto isr = space.load(regs::ISR);
		space.store(regs::ISR, isr);

		if ((isr & imr_isr::ROK) || (isr & imr_isr::RER)) {
			while (true) {
				auto& desc = rx_desc[rx_desc_ptr];
				if (desc.flags & rx_full_flags::OWN) {
					break;
				}

				auto size = desc.flags & rx_full_flags::FRAME_LEN;
				ethernet_process_packet(*this, to_virt<void>(desc.buffer), size);

				desc.flags = {rx_empty_flags::OWN(true) | rx_empty_flags::BUFFER_SIZE(PAGE_SIZE)};
				if (rx_desc_ptr == desc_count - 1) {
					desc.flags |= rx_empty_flags::EOR(true);
				}

				rx_desc_ptr = (rx_desc_ptr + 1) % desc_count;
			}
		}
		else if (isr & imr_isr::TER) {
			println("[kernel][nic]: rtl packet send error");
		}
		else if (isr & imr_isr::LINK_CHG) {
			println("[kernel][nic]: rtl link change");
			auto state = space.load(regs::PHY_STS) & phy_sts::LINK_STS;
			if (state) {
				println("[kernel][nic]: cable connected, starting auto-negotiation");
				auto bmcr = phy_read(phy_regs::BMCR);
				bmcr |= bmcr::RESTART_AN(true);
				phy_write(phy_regs::BMCR, bmcr);

				while (!(phy_read(phy_regs::BMSR) & bmsr::AUTO_NEG_COMPLETE));
				println("[kernel][nic]: done");
				space.store(regs::ISR, imr_isr::LINK_CHG(true));

				dhcp_discover(this);
			}
			else {
				ip = 0;
				ip_available_event.reset();
			}
		}

		return true;
	}

	IoSpace space;
	pci::Device& device;
	IrqHandler irq_handler {
		.fn = [this](IrqFrame*) {
			return on_irq();
		},
		.can_be_shared = false
	};
	TxDescriptor* tx_desc {};
	RxDescriptor* rx_desc {};
	u32 tx_desc_ptr {};
	u32 rx_desc_ptr {};
	u32 desc_count {};
	bool is_8139 {};
};

static bool rtl_init(pci::Device& device) {
	auto rtl = kstd::make_shared<Rtl>(device);
	{
		IrqGuard irq_guard {};
		auto guard = NICS->lock();
		guard->push(rtl);
	}

	rtl->init();
	println("[kernel][nic]: rtl init done");
	dhcp_discover(rtl.data());

	return true;
}

static PciDriver RTL_DRIVER {
	.init = rtl_init,
	.match = PciMatch::Device,
	.devices {
		{.vendor = 0x10EC, .device = 0x8139},
		{.vendor = 0x10EC, .device = 0x8168}
	}
};

PCI_DRIVER(RTL_DRIVER);
