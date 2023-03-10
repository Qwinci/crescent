#include "arch.hpp"
#include "console.hpp"
#include "dhcp.hpp"
#include "drivers/dev.hpp"
#include "ethernet.hpp"
#include "io.hpp"
#include "ip.hpp"
#include "memory/std.hpp"

#define REG_IDR0 0
#define REG_MAR0 0x8
#define REG_MAR1 0x9
#define REG_MAR2 0xA
#define REG_MAR3 0xB
#define REG_MAR4 0xC
#define REG_MAR5 0xD
#define REG_MAR6 0xE
#define REG_MAR7 0xF
#define REG_DTCCR 0x10
#define REG_TNPDS 0x20
#define REG_THPDS 0x28
#define REG_CR 0x37
#define REG_TPPOLL 0x38
#define REG_IMR 0x3C
#define REG_ISR 0x3E
#define REG_TCR 0x40
#define REG_RCR 0x44
#define REG_TCTR 0x48
#define REG_MPC 0x4C
#define REG_9346CR 0x50
#define REG_CONFIG0 0x51
#define REG_CONFIG1 0x52
#define REG_CONFIG2 0x53
#define REG_CONFIG3 0x54
#define REG_CONFIG4 0x55
#define REG_CONFIG5 0x56
#define REG_TIMERINT 0x58
#define REG_PHYAR 0x60
#define REG_TBICSR0 0x64
#define REG_TBI_ANAR 0x68
#define REG_TBI_LPAR 0x6A
#define REG_PHYSTATUS 0x6C
#define REG_WAKEUP0 0x84
#define REG_WAKEUP1 0x8C
#define REG_WAKEUP2LD 0x94
#define REG_WAKEUP2HD 0x9C
#define REG_WAKEUP3LD 0xA4
#define REG_WAKEUP3HD 0xAC
#define REG_WAKEUP4LD 0xB4
#define REG_WAKEUP4HD 0xBC
#define REG_CRC0 0xC4
#define REG_CRC1 0xC6
#define REG_CRC2 0xC8
#define REG_CRC3 0xCA
#define REG_CRC4 0xCC
#define REG_RMS 0xDA
#define REG_CCR 0xE0
#define REG_RDSAR 0xE4
#define REG_MTPS 0xEC

/// Reset
#define CMD_RST (1 << 4)
/// Receiver Enable
#define CMD_RE (1 << 3)
/// Transmitter Enable
#define CMD_TE (1 << 2)

#define TCR_IFG_NORMAL (0b11 << 24)
#define TCR_MXDMA_UNLIMITED (0b111 << 8)

#define RCR_RXFTH_NO_THRESHOLD (0b111 << 13)
#define RCR_MXDMA_UNLIMITED (0b111 << 8)
/// Accept Error Packet
#define RCR_AER (1 << 5)
/// Accept All Packets with Destination Address
#define RCR_AAP (1 << 0)

#define CMD_93C56_EEM_CONF_WRITE (0b11 << 6)

struct Descriptor {
	union {
		struct {
			u32 buf_size : 14;
			u32 reserved : 16;
			u32 eor : 1;
			u32 own : 1;
		} init;
		struct {
			u32 buf_size : 14;
			// TCP Checksum Failure
			u32 tcpf : 1;
			// UDP Checksum Failure
			u32 udpf : 1;
			// IP Checksum Failure
			u32 ipf : 1;
			// Protocol ID
			enum : u32 {
				PID_NON_IP = 0b00,
				PID_TCP = 0b01,
				PID_UDP = 0b10,
				PID_IP = 0b11
			} pid : 2;
			// CRC Error
			u32 crc : 1;
			// Runt Packet
			u32 runt : 1;
			// Receive Error Summary
			u32 res : 1;
			// Receive Watchdog Timer Expired
			u32 rwt : 1;
		private:
			u32 reserved : 2;
		public:
			// Broadcast Address Received
			u32 bar : 1;
			// Physical Address Matched
			u32 pam : 1;
			// Multicast Address Packet Received
			u32 mar : 1;
			// Last Segment Descriptor
			u32 ls : 1;
			// First Segment Descriptor
			u32 fs : 1;
			// End of Rx Descriptor Ring
			u32 eor : 1;
			// Ownership
			u32 own : 1;
		} filled_rx;
		struct {
			u32 buf_size : 16;
			// TCP checksum offload
			u32 tcpcs : 1;
			// UDP checksum offload
			u32 udpcs : 1;
			// IP checksum offload
			u32 ipcs : 1;
		private:
			u32 reserved : 8;
		public:
			// large send
			u32 lgsen : 1;
			// Last Segment Descriptor
			u32 ls : 1;
			// First Segment Descriptor
			u32 fs : 1;
			// End of Rx Descriptor Ring
			u32 eor : 1;
			// Ownership
			u32 own : 1;
		} filled_tx;
	};

	u32 vlan;
	u64 buf_addr;
};
static_assert(sizeof(Descriptor) == 16);

static Descriptor* rx_descriptors;
static Descriptor* tx_descriptors;
static volatile usize rx_index = 0;
static volatile usize tx_index = 0;
constexpr usize DESC_COUNT = PAGE_SIZE / sizeof(Descriptor);
constexpr usize RECEIVE_BUF_SIZE = 2048;
constexpr usize TRANSMIT_BUF_SIZE = 4096;

static bool init_descriptors() {
	auto rx_addr = PhysAddr {PAGE_ALLOCATOR.alloc_new()};
	if (!rx_addr.as_usize()) {
		return false;
	}
	auto tx_addr = PhysAddr {PAGE_ALLOCATOR.alloc_new()};
	if (!tx_addr.as_usize()) {
		PAGE_ALLOCATOR.dealloc_new(cast<void*>(rx_addr.as_usize()));
		return false;
	}

	rx_descriptors = as<Descriptor*>(as<void*>(rx_addr.to_virt()));
	memset(rx_descriptors, 0, PAGE_SIZE);
	tx_descriptors = as<Descriptor*>(as<void*>(tx_addr.to_virt()));
	memset(tx_descriptors, 0, PAGE_SIZE);

	for (usize i = 0; i < DESC_COUNT; ++i) {
		auto& tx_desc = tx_descriptors[i];
		auto& rx_desc = rx_descriptors[i];

		tx_desc.filled_tx.fs = 1;
		tx_desc.filled_tx.ls = 1;

		rx_desc.filled_rx.own = 1;

		auto tx_buf = new u8[TRANSMIT_BUF_SIZE];
		auto rx_buf = new u8[RECEIVE_BUF_SIZE];

		tx_desc.buf_addr = VirtAddr {tx_buf}.to_phys().as_usize();
		rx_desc.buf_addr = VirtAddr {rx_buf}.to_phys().as_usize();
		rx_desc.filled_rx.buf_size = RECEIVE_BUF_SIZE;
	}

	tx_descriptors[DESC_COUNT - 1].filled_tx.eor = 1;
	rx_descriptors[DESC_COUNT - 1].filled_rx.eor = 1;

	return true;
}

static u8 rtl_int = 0;

/// System Error
#define INT_SERR (1 << 15)
#define INT_TIMEOUT (1 << 14)
/// Software Interrupt
#define INT_SWINT (1 << 8)
/// Tx Descriptor Unavailable
#define INT_TDU (1 << 7)
/// Rx FIFO Overflow
#define INT_FOVW (1 << 6)
/// Link Change
#define INT_LINKCHG (1 << 5)
/// Rx Descriptor Unavailable
#define INT_RDU (1 << 4)
/// Transmit Error
#define INT_TER (1 << 3)
/// Transmit OK
#define INT_TOK (1 << 2)
/// Receive Error
#define INT_RER (1 << 1)
/// Receive OK
#define INT_ROK (1 << 0)

/// Basic mode control register
#define PHY_BMCR 0
/// Basic mode status register
#define PHY_BMSR 1

#define BMSR_EXTENDED_CAP (1 << 0)
#define BMSR_JABBER_DETECH (1 << 1)
#define BMSR_LINK_STATUS (1 << 2)
#define BMSR_AUTO_NEG_ABILITY (1 << 3)
#define BMSR_REMOTE_FAULT (1 << 4)
#define BMSR_AUTO_NEG_COMPLETE (1 << 5)
#define BMSR_PREAMBLE_SUPPRESS (1 << 6)

/// Phy Reset
#define BMCR_RESET (1 << 15)
/// Auto-Negotiation Enable
#define BMCR_ANE (1 << 12)
/// Restart Auto-Negotiation
#define BMCR_RESTART_AN (1 << 9)

constexpr usize MAX_SEND_SIZE = 3968;
static u16 base = 0;

#define TTPOLL_HIGH_PRIORITY (1 << 7)
#define TPPOLL_NORMAL_PRIORITY (1 << 6)

static void rtl_send_packet(void* buffer, usize size) {
	if (size > MAX_SEND_SIZE) {
		println("rtl_send_packet: packet too large!");
		return;
	}

	auto flags = enter_critical();

	volatile auto* desc = &tx_descriptors[tx_index];

	while (desc->filled_tx.own) {
		// todo sched sleep
		udelay(1000);
	}

	desc->filled_tx.buf_size = size;
	memcpy(PhysAddr {desc->buf_addr}.to_virt(), buffer, size);
	desc->filled_tx.own = 1;

	out1(base + REG_TPPOLL, TPPOLL_NORMAL_PRIORITY);

	tx_index = (tx_index + 1) % DESC_COUNT;

	leave_critical(flags);
}

static Nic rtl_nic {.send = rtl_send_packet};

static void rtl_int_handler(InterruptCtx*, void*) {
	arch_eoi();

	auto status = in2(base + REG_ISR);

	out2(base + REG_ISR, 0b1100000111111111);

	if (status & INT_ROK) {
		for (usize i = 0; i < DESC_COUNT; ++i) {
			auto& desc = rx_descriptors[i];
			if (!desc.filled_rx.own) {
				auto addr = PhysAddr {desc.buf_addr}.to_virt();
				auto size = desc.filled_rx.buf_size;
				auto data = as<u8*>(as<void*>(addr));
				if (!ethernet_process_packet(&rtl_nic, data, size)) {
					println("rtl: discarding unprocessed packet");
				}
				desc.filled_rx.buf_size = RECEIVE_BUF_SIZE;
				desc.filled_rx.own = 1;
			}
		}
	}
	else if (status & INT_TOK);
	else {
		print("rtl status: ");
		if (status & INT_SERR) {
			println("System Error");
		}
		else if (status & INT_TIMEOUT) {
			println("Timeout");
		}
		else if (status & INT_SWINT) {
			println("Software Int");
		}
		else if (status & INT_TDU) {
			println("Tx Descriptor Unavailable");
		}
		else if (status & INT_FOVW) {
			println("Rx FIFO Overflow");
		}
		else if (status & INT_LINKCHG) {
			println("Link Change");
		}
		else if (status & INT_RDU) {
			println("Rx Descriptor Unavailable");
		}
		else if (status & INT_TER) {
			println("Transmit Error");
		}
		else if (status & INT_RER) {
			println("Receive Error");
		}
	}
}

#define PHYAR_FLAG (1 << 31)
#define PHYAR_REG_SHIFT 16
#define PHYAR_DATA_SHIFT 0
#define PHYAR_DATA_MASK 0xFFFF

static u16 phy_read16(u8 reg) {
	u32 tmp = as<u32>(reg) << PHYAR_REG_SHIFT;
	out4(base + REG_PHYAR, tmp);

	for (u8 i = 0; i < 5; ++i) {
		udelay(1000);
		tmp = in4(base + REG_PHYAR);
		if (tmp & PHYAR_FLAG) {
			break;
		}
	}

	return tmp & PHYAR_DATA_MASK;
}

static void phy_write16(u8 reg, u16 value) {
	u32 tmp = as<u32>(reg) << PHYAR_REG_SHIFT | as<u32>(value) << PHYAR_DATA_SHIFT;
	out4(base + REG_PHYAR, tmp);

	for (u8 i = 0; i < 5; ++i) {
		udelay(1000);
		if (!(in4(base + REG_PHYAR) & PHYAR_FLAG)) {
			break;
		}
	}
}

void init_rtl8169(Pci::Header0* hdr) {
	println("rtl8169 initializing");

	if (!rtl_int) {
		rtl_int = alloc_int_handler(rtl_int_handler);
		if (!rtl_int) {
			println("rtl: failed to allocate interrupt");
			return;
		}
		hdr->set_irq(0, rtl_int);
		hdr->enable_irqs();
	}

	hdr->common.command |= Pci::Cmd::IoSpace | Pci::Cmd::MemSpace | Pci::Cmd::BusMaster
		| Pci::Cmd::MemWriteInvalidateEnable;
	
	base = hdr->get_io_bar(0);

	u8 mac[6];
	for (u8 i = 0; i < 6; ++i) {
		mac[i] = in1(base + REG_IDR0 + i);
	}

	println("mac: ", Fmt::HexNoPrefix, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], Fmt::Dec);

	out1(base + REG_CR, CMD_RST);

	while (in1(base + REG_CR) & CMD_RST) {
		io_wait();
	}

	println("chip reset done");

	auto bmsr = phy_read16(PHY_BMSR);
	if (bmsr & BMSR_AUTO_NEG_ABILITY) {
		if (bmsr & BMSR_LINK_STATUS) {
			println("performing card auto-negotiation");
			phy_write16(PHY_BMCR, BMCR_ANE | BMCR_RESTART_AN);
			while ((phy_read16(PHY_BMCR) & BMCR_RESTART_AN)) {
				udelay(1000);
			}
			println("auto-negotiation done");
		}
		else {
			println("link is not up, not performing auto-negotiation");
		}
	}
	else {
		println("card does not support auto-negotiation, leaving it as is");
	}

	hdr->common.command |= Pci::Cmd::IoSpace | Pci::Cmd::MemSpace | Pci::Cmd::BusMaster
						   | Pci::Cmd::MemWriteInvalidateEnable;

	println("initializing descs");

	auto status = init_descriptors();

	if (!status) {
		println("failed to initialize rx descs");
		return;
	}

	out1(base + REG_9346CR, CMD_93C56_EEM_CONF_WRITE);

	out1(base + REG_CR, CMD_TE);
	out4(base + REG_TCR, TCR_IFG_NORMAL | TCR_MXDMA_UNLIMITED);

	//out4(base + REG_RCR, RCR_RXFTH_NO_THRESHOLD | RCR_MXDMA_UNLIMITED /*| RCR_AER*/ | RCR_AAP);
	out4(base + REG_RCR, RCR_RXFTH_NO_THRESHOLD | RCR_MXDMA_UNLIMITED | 0b1 | 1 << 1 | 1 << 2 | 1 << 3);

	out1(base + REG_MTPS, 0x3B);
	out2(base + REG_RMS, 0x1FFF);

	auto tx_phys = VirtAddr {tx_descriptors}.to_phys().as_usize();
	auto rx_phys = VirtAddr {rx_descriptors}.to_phys().as_usize();

	out4(base + REG_TNPDS, tx_phys);
	out4(base + REG_TNPDS + 4, tx_phys >> 32);

	out4(base + REG_RDSAR, rx_phys);
	out4(base + REG_RDSAR + 4, rx_phys >> 32);

	out2(base + REG_IMR, 0b1100000111111111);

	//out4(base + REG_TIMERINT, 0);

	out1(base + REG_CR, CMD_RE | CMD_TE);

	out1(base + REG_9346CR, 0);

	memcpy(rtl_nic.mac, mac, sizeof(mac));

	println("rtl8169 initialized!");

	dhcp_discover(&rtl_nic);
}

static PciDriver pci_driver {
	.match = PciDriver::MATCH_DEV,
	.load = init_rtl8169,
	.dev_count = 6,
	.devices {
		{.vendor = 0x10EC, .device = 0x8161},
		{.vendor = 0x10EC, .device = 0x8168},
		{.vendor = 0x10EC, .device = 0x8169},
		{.vendor = 0x1259, .device = 0xC107},
		{.vendor = 0x1737, .device = 0x1032},
		{.vendor = 0x16EC, .device = 0x0116}
	}
};

PCI_DRIVER(rtl8169, pci_driver);