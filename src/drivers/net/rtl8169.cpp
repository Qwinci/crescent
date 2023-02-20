#include "arch/x86/lapic.hpp"
#include "console.hpp"
#include "drivers/dev.hpp"
#include "io.hpp"
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
	u32 buf_size : 14;
	u32 reserved : 16;
	u32 eor : 1;
	u32 own : 1;
	u32 vlan;
	u64 buf_addr;
};

static Descriptor* rx_descriptors;
static Descriptor* tx_descriptors;
static Descriptor* current_rx_desc;
static Descriptor* current_tx_desc;

static bool init_descriptors() {
	auto rx_addr = PhysAddr {PAGE_ALLOCATOR.alloc_new()};
	if (!rx_addr.as_usize()) {
		return false;
	}
	auto tx_addr = PhysAddr {PAGE_ALLOCATOR.alloc_new()};
	if (!tx_addr.as_usize()) {
		return false;
	}

	rx_descriptors = as<Descriptor*>(as<void*>(rx_addr.to_virt()));
	memset(rx_descriptors, 0, PAGE_SIZE);
	tx_descriptors = as<Descriptor*>(as<void*>(tx_addr.to_virt()));
	memset(tx_descriptors, 0, PAGE_SIZE);

	current_rx_desc = rx_descriptors;
	current_tx_desc = tx_descriptors;

	for (usize i = 0; i < PAGE_SIZE / sizeof(Descriptor); ++i) {
		auto& desc = rx_descriptors[i];
		if (i == PAGE_SIZE / sizeof(Descriptor) - 1) {
			desc.eor = 1;
		}
		desc.own = 1;

		auto buf = PhysAddr {PAGE_ALLOCATOR.alloc_new()};
		if (!buf.as_usize()) {
			// todo free allocated memory
			return false;
		}
		desc.buf_addr = buf.as_usize();
		desc.buf_size = PAGE_SIZE;
	}

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
// Receive OK
#define INT_ROK (1 << 0)

/// Phy Reset
#define BMCR_RESET (1 << 15)
/// Auto-Negotiation Enable
#define BMCR_ANE (1 << 12)
/// Restart Auto-Negotiation
#define BMCR_RESTART_AN (1 << 9)

static u16 base = 0;

static void rtl_int_handler(InterruptCtx*) {
	auto status = in2(base + REG_ISR);

	if (status & INT_ROK) {
		println("receive ok!");
		out2(base + REG_ISR, 0xFFFF);

		while (!current_rx_desc->own) {
			auto addr = PhysAddr {current_rx_desc->buf_addr}.to_virt();
			auto size = current_rx_desc->buf_size;
			auto data = as<const u8*>(as<void*>(addr));

			println("received buffer of size ", size, ", data: ", Fmt::HexNoPrefix);
			if (size <= 64) {
				u8 mac_dest[6];
				u8 mac_src[6];
				u8 tag_8021Q[4];
				u16 length;

				memcpy(mac_dest, data, 6);
				memcpy(mac_src, data + 6, 6);
				memcpy(tag_8021Q, data + 12, 4);
				length = as<u16>(data[16]) << 8 | data[17];

				print("dest: ");
				for (u8 i : mac_dest) {
					print(i);
				}
				print(" src: ");
				for (u8 i : mac_src) {
					print(i);
				}

				if (length <= 1500) {
					print(" length: ", Fmt::Dec, length, Fmt::HexNoPrefix, " payload: ");
					for (usize i = 0; i < length; ++i) {
						print(data[18 + i]);
					}
				}
				else {
					print(" type: ", ZeroPad(4), length, ZeroPad(0));
				}

				println(Fmt::Dec);
			}
			else {
				println("> too large to display");
			}

			if (!current_rx_desc->own) {
				current_rx_desc->own = 1;
			}

			if (current_rx_desc == rx_descriptors + PAGE_SIZE / sizeof(Descriptor) - 1) {
				current_rx_desc = rx_descriptors;
			}
			else {
				current_rx_desc += 1;
			}
		}
	}
	else {
		println("rtl status: ", Fmt::Bin, status);
	}

	Lapic::eoi();
}

void init_rtl8169(Pci::Header0* hdr) {
	println("rtl8169 initializing");
	bool legacy = true;
	if (auto msi = as<volatile Pci::MsiCap*>(hdr->get_cap(Pci::Cap::Msi))) {
		println("msi found");
		legacy = false;
		rtl_int = alloc_int_handler(rtl_int_handler);
		if (!rtl_int) {
			println("failed to allocate interrupt");
			return;
		}

		msi->set_max_int(1);
		msi->set_data(rtl_int);
		msi->set_cpu(0);
		msi->enable(true);
	}
	if (hdr->get_cap(Pci::Cap::MsiX)) {
		println("msix found");
		legacy = false;
	}

	if (legacy) {
		println("rtl8169: legacy interrupts are not supported");
		return;
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

	println("initializing descs");

	auto status = init_descriptors();

	if (!status) {
		println("failed to initialize rx descs");
		return;
	}

	//out1(base + REG_9346CR, CMD_93C56_EEM_CONF_WRITE);

	out1(base + REG_CR, CMD_TE);
	out4(base + REG_TCR, TCR_IFG_NORMAL | TCR_MXDMA_UNLIMITED);

	out4(base + REG_RCR, RCR_RXFTH_NO_THRESHOLD | RCR_MXDMA_UNLIMITED | RCR_AER | RCR_AAP);

	// 3968 max tx size
	out1(REG_MTPS, 0x1F);
	// 0x1000 max rx size
	out2(REG_RMS, PAGE_SIZE);

	auto tx_phys = VirtAddr {tx_descriptors}.to_phys().as_usize();
	auto rx_phys = VirtAddr {rx_descriptors}.to_phys().as_usize();

	out4(base + REG_TNPDS, tx_phys);
	out4(base + REG_TNPDS + 4, tx_phys >> 32);

	out4(base + REG_RDSAR, rx_phys);
	out4(base + REG_RDSAR + 4, rx_phys >> 32);

	out2(base + REG_IMR, 0b1100000111111111);

	out1(base + REG_CR, CMD_RE | CMD_TE);

	//out1(base + REG_9346CR, 0);

	hdr->common.command &= ~Pci::Cmd::InterruptDisable;

	println("rtl8169 initialized!");
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