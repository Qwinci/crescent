#include "drivers/dev.hpp"

struct Caps {
	u8 caplength;
	u8 reserved1;
	u16 hciversion;
	struct {
		u32 max_slots : 8;
		u32 max_intrs : 11;
		u32 reserved : 5;
		u32 max_ports : 8;
	} hcsparams1;
	struct {
		u32 ist : 4;
		u32 erst_max : 4;
		u32 reserved : 13;
		u32 max_scratchpad_bufs_hi : 5;
		u32 spr : 1;
		u32 max_scratchpad_bufs_lo : 5;
	} hcsparams2;
	struct {
		u32 u1_dev_exit_latency : 8;
		u32 reserved : 8;
		u32 u2_dev_exit_latency : 16;
	} hcsparams3;
	struct {
		u32 ac64 : 1;
		u32 bnc : 1;
		u32 csz : 1;
		u32 ppc : 1;
		u32 pind : 1;
		u32 lhrc : 1;
		u32 ltc : 1;
		u32 nss : 1;
		u32 pae : 1;
		u32 spc : 1;
		u32 sec : 1;
		u32 cfc : 1;
		u32 max_psa_size : 4;
		u32 x_ecp : 16;
	} hccparams1;
	u32 dboff;
	u32 rtsoff;
	struct {
		u32 u3c : 1;
		u32 cmc : 1;
		u32 fsc : 1;
		u32 ctc : 1;
		u32 lec : 1;
		u32 cic : 1;
		u32 etc : 1;
		u32 etc_tsc : 1;
		u32 gsc : 1;
		u32 vtc : 1;
		u32 reserved : 22;
	} hccparams2;
};

#define USBCMD_RUN (1 << 0)
/// Host Controller Reset
#define USBCMD_HCRST (1 << 1)
/// Interrupter Enable
#define USBCMD_INTE (1 << 2)
/// Host System Error Enable
#define USBCMD_HSEE (1 << 3)
/// Light Host Controller Reset
#define USBCMD_LHCRST (1 << 7)
/// Controller Save State
#define USBCMD_CSS (1 << 8)
/// Controller Restore State
#define USBCMD_CRS (1 << 9)
/// Enable Wrap Event
#define USBCMD_EWE (1 << 10)
/// Enable U3 MFINDEX Stop
#define USBCMD_EU3S (1 << 11)
/// CEM Enable
#define USBCMD_CME (1 << 13)
/// Extended TBC Enable
#define USBCMD_ETE (1 << 14)
/// Extended TBC TRB Status Enable
#define USBCMD_TSC_EN (1 << 15)
/// VTIO Enable
#define USBCMD_VTIOE (1 << 16)

struct Operational {
	u32 usbcmd;
	u32 usbsts;
	u32 pagesize;
	u32 reserved1[2];
	u32 dnctrl;
	u32 crcr;
	u32 reserved2[4];
	union {
		u64 dcbaap;
		struct {
			u32 dcbaap_low;
			u32 dcbaap_high;
		};
	};
	u32 config;
	u32 reserved3[241];
	struct {
		u32 portsc;
		u32 portpmsc;
		u32 portli;
		u32 porthlpmc;
	} ports[];
};

struct Runtime {
	u32 mfindex;
	u32 reserved[7];
	struct {
		u32 iman;
		u32 imod;
		u32 erstsz;
		u32 reserved;
		union {
			u64 erstba;
			struct {
				u32 erstba_low;
				u32 erstba_high;
			};
		};
		union {
			u64 erdp;
			struct {
				u32 erdp_low;
				u32 erdp_high;
			};
		};
	} interrupters[];
};

static void xhci_init(Pci::Header0* hdr) {
	println("xhci init");
	auto version = *(cast<u8*>(hdr) + 0x60);
	println("version: ", Fmt::HexNoPrefix, version, Fmt::Dec);

	hdr->common.cache_line = 64;
	hdr->common.command |= Pci::Cmd::IoSpace | Pci::Cmd::MemSpace | Pci::Cmd::BusMaster;

	if (hdr->is_io_space(0)) {
		println("xhci: io space is not supported");
		return;
	}

	auto addr = PhysAddr {hdr->map_bar(0)}.to_virt().as_usize();

}

static PciDriver pci_dev {
	.match = PciDriver::MATCH_CLASS | PciDriver::MATCH_SUBCLASS | PciDriver::MATCH_PROG,
	.dev_class = 0xC,
	.dev_subclass = 0x3,
	.dev_prog = 0x30,
	.load = xhci_init
};

PCI_DRIVER(xhci, pci_dev);