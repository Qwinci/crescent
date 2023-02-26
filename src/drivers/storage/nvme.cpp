#include "arch.hpp"
#include "console.hpp"
#include "drivers/dev.hpp"
#include "drivers/pci.hpp"
#include "timer/timer.hpp"
#include "utils/math.hpp"

#define SUBMISSION_Q_TAIL_DOORBELL(x, stride) (0x1000 + 2 * (x) * (4 << (stride)))
#define COMPLETION_Q_HEAD_DOORBELL(x, stride) (0x1000 + (2 * (x) + 1) * (4 << (stride)))

/// Controller Capabilities
#define REG64_CAP 0
/// Controller Ready Modules Supported
#define CAP_CRMS(r) (r >> 59 & 0b11)
/// Controller Ready With Media Support
#define CRMS_CRWMS 0b1
/// Controller Ready Independent of Media Support
#define CRMS_CRIMS (1 << 1)
/// NVM Subsystem Shutdown Supported
#define CAP_NSSS(r) (r >> 58 & 1)
/// Controller Memory Buffer Supported
#define CAP_CMBS(r) (r >> 57 & 1)
/// Persistent Memory Region Supported
#define CAP_PMRS(r) (r >> 56 & 1)
/// Memory Page Size Maximum (2 ^ (12 + size))
#define CAP_MPSMAX(r) (r >> 52 & 0b1111)
/// Memory Page Size Minimum (2 ^ (12 + size))
#define CAP_MPSMIN(r) (r >> 48 & 0b1111)
/// Controller Power Scope
#define CAP_CPS(r) (r >> 46 & 0b11)
#define CPS_NOT_REPORTED 0
#define CPS_CONTROLLER_SCOPE 1
#define CPS_DOMAIN_SCOPE 0b10
#define CPS_NVM_SUBSYS_SCOPE 0b11
/// Boot Partition Support
#define CAP_BPS(r) (r >> 45 & 1)
/// Command Set Supported
#define CAP_CSS(r) (r >> 37 & 0xFF)
/// Only Admin Command Set supported
#define CSS_NO_IO (1 << 7)
/// Supports one or more IO Command Sets
#define CSS_IO (1 << 6)
/// NVM Command Set
#define CSS_NVM (1)
/// NVM Subsystem Reset Supported
#define CAP_NSSRS(r) (r >> 36 & 1)
/// Doorbell Stride (2 ^ (2 + stride))
#define CAP_DSTRD(r) (r >> 32 & 0b1111)
/// Timeout (in 500ms units)
#define CAP_TO(r) (r >> 24 & 0xFF)
/// Arbitration Mechanism Supported
#define CAP_AMS(r) (r >> 17 & 0b11)
/// Weighted Round Robin with Urgent Priority Class
#define AMS_WEIGHTED_RR 1
/// Vendor Specific
#define AMS_VENDOR (1 << 1)
/// Contiguous Queues Required
#define CAP_CQR(r) (r >> 16 & 1)
/// Maximum Queue Entries Supported (1 == 2 queues)
#define CAP_MQES(r) (r & 0xFFFF)

/// Version
#define REG32_VS 0x8
/// Major Version Number
#define VS_MJR(r) (r >> 16 & 0xFFFF)
/// Minor Version Number
#define VS_MNR(r) (r >> 8 & 0xFF)
/// Tertiary Version Number
#define VS_TER(r) (r & 0xFF)

/// Interrupt Mask Set
#define REG32_INTMS 0xC
/// Interrupt Mask Clear
#define REG32_INTMC 0xF

/// Controller Configuration
#define REG32_CC 0x14
/// Controller Ready Independent of Media Enable
#define CC_CRIME(r) (r >> 24 & 1)
#define CC_CRIME_MASK 0b1
#define CC_CRIME_V(v) ((v) << 24)
/// IO Completion Queue Entry Size (power of two)
#define CC_IOCQES(r) (r >> 20 & 0b1111)
#define CC_IOCQES_MASK 0b1111
#define CC_IOCQES_V(v) ((v) << 20)
/// IO Submission Queue Entry Size (power of two)
#define CC_IOSQEC(r) (r >> 16 & 0b1111)
#define CC_IOSQEC_MASK 0b1111
#define CC_IOSQEC_V(v) ((v) << 16)
/// Shutdown Notification
#define CC_SHN(r) (r >> 14 & 0b11)
#define SHN_NO_NOTIF 0
#define SHM_NORMAL_SHUTDOWN_NOTIF 1
#define SHM_ABRUPT_SHUTDOWN_NOTIF 0b11
#define CC_SHN_MASK 0b11
#define CC_SHN_V(v) ((v) << 14)
/// Arbitration Mechanism Selected
#define CC_AMS(r) (r >> 11 & 0b111)
/// Round Robin
#define AMS_RR 0
#define CC_AMS_MASK 0b111
#define CC_AMS_V(v) ((v) << 11)
/// Memory Page Size (2 ^ (12 + size))
#define CC_MPS(r) (r >> 7 & 0b1111)
#define CC_MPS_MASK 0b1111
#define CC_MPS_V(v) ((v) << 7)
/// IO Command Set Selected
#define CC_CSS(r) (r >> 4 & 0b111)
/// All Supported IO Command Sets
#define CSS_ALL_IO (0b110)
/// Admin Command Set Only
#define CSS_ADMIN_ONLY 0b111
#define CC_CSS_MASK 0b111
#define CC_CSS_V(v) ((v) << 4)
/// Enable
#define CC_EN(r) (r & 1)
#define CC_EN_MASK 1
#define CC_EN_V(v) ((v))

/// Controller Status
#define REG32_CSTS 0x1C
/// Shutdown Type
#define CSTS_ST(r) (r >> 6 & 1)
#define ST_CONTROLLER 0
#define ST_NVM_SUBSYS 1
/// Processing Paused
#define CSTS_PP(r) (r >> 6 & 1)
/// NVM Subsystem Reset Occurred
#define CSTS_NSSRO(r) (r >> 4 & 1)
#define CSTS_NSSRO_CLEAR (1 << 4)
/// Shutdown Status
#define CSTS_SHST(r) (r >> 2 & 0b11)
/// Normal operation
#define SHST_NORMAL 0
/// Shutdown processing occurring
#define SHST_OCCURRING 1
/// Shutdown processing complete
#define SHST_COMPLETE 0b10
/// Controller Fatal Status
#define CSTS_CFS(r) (r >> 1 & 1)
/// Ready
#define CSTS_RDY(r) (r & 1)

/// NVM Subsystem Reset
#define REG32_NSSR 0x20
#define NSSR_NSSRC_INIT 0x4E564D65

/// Admin Queue Attributes
#define REG32_AQA 0x24
/// Admin Completion Queue Size
#define AQA_ACQS(r) (r >> 16 & 0xFFF)
#define AQA_ACQS_MASK 0xFFF
#define AQA_ACQS_V(v) ((v) << 16)
/// Admin Submission Queue Size
#define AQA_ASQS(r) (r & 0xFFF)
#define AQA_ASQS_MASK 0xFFF
#define AQA_ASQS_V(v) ((v))

/// Admin Submission Queue Base
#define REG64_ASQ 0x28
#define ASQ_MASK ~(0xFFFULL)

/// Admin Completion Queue Base
#define REG64_ACQ 0x30
#define ACQ_MASK ~(0xFFFULL)

struct Regs {
	u64 cap;
	u32 vs;
	u32 intms;
	u32 intmc;
	u32 cc;
private:
	u32 reserved;
public:
	u32 csts;
	u32 nssr;
	u32 aqa;
	u64 asq;
	u64 acq;
};

enum class Op : u8 {
	DeleteIoSubQueue = 0,
	CreateIoSubQueue = 1,
	GetLogPage = 2,
	DeleteIoCompQueue = 4,
	CreateIoCompQueue = 5,
	Identify = 6,
	Abort = 8,
	SetFeatures = 9,
	GetFeatures = 0xA,
	AsyncEventRequest = 0xC,
	NamespaceManagement = 0xD,
	FirmwareCommit = 0x10,
	FirmwareImageDownload = 0x11,
	DeviceSelfTest = 0x14,
	NamespaceAttachment = 0x15,
	KeepAlive = 0x18,
	DirectiveSend = 0x19,
	DirectiveReceive = 0x1A,
	VirtualizationManagement = 0x1C,
	NvmeMiSend = 0x1D,
	NvmeMiReceive = 0x1E,
	CapabilityManagement = 0x20,
	Lockdown = 0x24,
	DoorbellBufferConfig = 0x7C,
	FabricsCommands = 0x7F,
	FormatNvm = 0x80,
	SecuritySend = 0x81,
	SecurityReceive = 0x82,
	Sanitize = 0x84,
	GetLbaStatus = 0x86
};

enum class Cns : u8 {
	Namespace = 0,
	Controller = 1,
	NamespaceList = 2
};

enum class QueuePriority : u32 {
	Urgent = 0,
	High = 0b1,
	Medium = 0b10,
	Low = 0b11
};

struct Cmd {
	Op op;
	u8 flags;
	u16 cid;
	union {
		struct [[gnu::packed]] {
			u32 nsid;
			u32 cmd2;
			u32 cmd3;
			u64 mptr;
			u64 dtpr[2];
			u32 cmd10;
			u32 cmd11;
			u32 cmd12;
			u32 cmd13;
			u32 cmd14;
			u32 cmd15;
		} common;
		struct [[gnu::packed]] {
			u32 nsid;
			u32 reserved1[2];
			u64 mtpr;
			u64 dtpr[2];
			Cns cns;
			u8 reserved2;
			u16 cntid;
			u16 cns_ident;
			u8 reserved3;
			u8 csi;
		} identify;
		struct [[gnu::packed]] {
			u32 reserved1[3];
			u64 mtpr;
			u64 dtpr[2];
			u16 qid;
			u16 qsize;
			// Physically Contiguous
			u32 pc : 1;
			// Interrupts Enabled
			u32 ien : 1;
			u32 reserved2 : 16;
			// Interrupt Vector
			u32 iv : 16;
		} create_io_comp_queue;
		struct [[gnu::packed]] {
			u32 reserved1[3];
			u64 mtpr;
			u64 dtpr[2];
			u16 qid;
			u16 qsize;
			// Physically Contiguous
			u32 pc : 1;
			// Queue Priority
			QueuePriority qrio : 2;
			u32 reserved2 : 13;
			// Completion Queue Identifier
			u32 cqid : 16;
		} create_io_sub_queue;
		struct [[gnu::packed]] {
			u32 nsid;
			u32 reserved1[2];
			u64 mtpr;
			u64 dtpr[2];
			// Starting LBA
			u64 slba;
			// Number of Logical Blocks (-1)
			u16 nlb;
			u8 reserved2;
			// Storage Tag Check
			u8 stc : 1;
			u8 reserved3 : 1;
			// Protection Information Field
			u8 prinfo : 4;
			// Force Unit Access
			u8 fua : 1;
			// Limited Retry
			u8 lr : 1;
		} io_read;
		struct [[gnu::packed]] {
			u32 nsid;
			u32 reserved1[2];
			u64 mtpr;
			u64 dtpr[2];
			// Starting LBA
			u64 slba;
			// Number of Logical Blocks (-1)
			u16 nlb;
			u8 reserved2 : 4;
			// Directive Type
			u8 dtype : 4;
			// Storage Tag Check
			u8 stc : 1;
			u8 reserved3 : 1;
			// Protection Information Field
			u8 prinfo : 4;
			// Force Unit Access
			u8 fua : 1;
			// Limited Retry
			u8 lr : 1;
		} io_write;
	};
};
static_assert(sizeof(Cmd) == 64);

struct CompletionEntry {
	u32 cmd_specific1;
	u32 cmd_specific2;
	/// SQ Head Pointer
	u16 sqhd;
	/// SQ Identifier
	u16 sqid;
	/// Command Identifier
	u16 cid;
	/// Phase
	u16 p : 1;
	u16 status : 15;
};

struct [[gnu::packed]] NamespaceListIdentDescriptor {
	enum : u8 {
		NIDT_RESERVED = 0,
		NIDT_IEEE = 1,
		NIDT_GUID = 2,
		NIDT_UUID = 3,
		NIDT_CSI = 4
	} nidt;
	// Namespace Identifier Length
	u8 nidl;
	u8 reserved[2];
	union {
		u8 csi;
		u64 ieee;
		u64 uuid[2];
	};
};

struct [[gnu::packed]] NamespaceIdent {
	// Namespace Size in logical blocks
	u64 nsze;
	// Namespace Capacity
	u64 ncap;
	// Namespace Utilization
	u64 nuse;
	// Namespace Features
	struct {
		// Thin Provisioning
		u8 thinp : 1;
		// NAWUN, NAWUPF, NACWU
		u8 nsabp : 1;
		// Deallocated or Unwritten Logical Block Error
		u8 dae : 1;
		u8 uidreuse : 1;
		// NPWG, NPWA, NPDG, NPDA
		u8 optperf : 1;
		u8 reserved : 3;
	} nsfeat;
	// Number of LBA Formats
	u8 nlbaf;
	// Formatted LBA Size
	struct {
		u8 fmt_index_low : 4;
		u8 extended_metadata : 1;
		u8 fmt_index_high : 2;
		u8 reserved : 1;
	} flbas;
	// Metadata Capabilities
	struct {
		u8 extended_metadata_support : 1;
		u8 separate_metadata_support : 1;
		u8 reserved : 6;
	} mc;
	// End-to-end Data Protection Capabilities
	struct {
		// Protection Information Type 1 Supported
		u8 pit1s : 1;
		// Protection Information Type 2 Supported
		u8 pit2s : 1;
		// Protection Information Type 3 Supported
		u8 pit3s : 1;
		// Protection Information In First Bytes
		u8 piifb : 1;
		// Protection Information In Last Bytes
		u8 piilb : 1;
		u8 reserved : 3;
	} dpc;
	// End-to-end Data Protection Type Settings
	struct {
		// Protection Information Type
		u8 pit : 3;
		// Protection Information Position
		u8 pip : 1;
		u8 reserved : 4;
	} dps;
	// Namespace Multi-path IO and Namespace Sharing Capabilities
	u8 nmic;
	// Reservation Capabilities
	u8 rescap;
	// Format Progress Indicator
	u8 fpi;
	// Deallocate Logical Block Features
	struct {
		u8 behaviour : 3;
		u8 deallocate_write_zero : 1;
		u8 guard_crc : 1;
		u8 reserved : 3;
	} dlfeat;
	// Namespace Atomic Write Unit Normal
	u16 nawun;
	// Namespace Atomic Write Unit Power Fail
	u16 nawupf;
	// Namespace Atomic Compare & Write Unit
	u16 nacwu;
	// Namespace Atomic Boundary Size Normal
	u16 nabsn;
	// Namespace Atomic Boundary Offset
	u16 nabo;
	// Namespace Atomic Boundary Size Power Fail
	u16 nabspf;
	// Namespace Optimal IO Boundary
	u16 noiob;
	// NVM Capacity in bytes
	u64 nvmcap[2];
	// Namespace Preferred Write Granularity
	u16 npwg;
	// Namespace Preferred Write Alignment
	u16 npwa;
	// Namespace Preferred Deallocate Granularity
	u16 npdg;
	// Namespace Preferred Deallocate Alignment
	u16 npda;
	// Namespace Optimal Write Size
	u16 nows;
	// Maximum Single Source Range Length
	u16 mssrl;
	// Maximum Copy Length
	u32 mcl;
	// Maximum Source Range Count
	u8 msrc;
	u8 reserved1[11];
	// ANA Group Identifier
	u32 anagrpid;
	u8 reserved2[3];
	// Namespace Attributes
	struct {
		u8 write_protected : 1;
		u8 reserved : 7;
	} nsattr;
	// NVM Set Identifier
	u16 nvmsetid;
	// Endurance Group Identifier
	u16 endgid;
	u64 nguid[2];
	u64 eui64;
	struct {
		// Metadata size
		u16 ms;
		// LBA Data Size (power of two)
		u8 lbads;
		enum : u8 {
			RP_BEST_PERF = 0,
			RP_BETTER_PERF = 0b1,
			RP_GOOD_PERF = 0b10,
			RP_DEGRADED_PERF = 0b11
		} rp : 2;
	} formats[];
};

static u8 nvme_vec {};

struct NvmeController {
	static void irq(InterruptCtx* ctx, void* s) {
		auto& self = *as<NvmeController*>(s);
		println("nvme irq");

		auto index = self.admin_comp_queue_i;

		const auto& entry = self.admin_sub_queue[self.admin_comp_queue[index].sqhd - 1];
		auto addr = PhysAddr {entry.identify.dtpr[0]}.to_virt();

		auto ptr = as<u8*>(as<void*>(addr));

		usize max_transfer_size = ptr[77];
		if (max_transfer_size == 0) {
			println("no max transfer size");
		}
		else {
			max_transfer_size = pow2(max_transfer_size);
			println("max transfer size: ", max_transfer_size, " pages");
		}

		auto type = ptr[111];
		if (type == 1) {
			println("io controller");
		}
		else {
			println("unknown nvme controller type ", type);
		}

		while (index < admin_comp_queue_size && self.admin_comp_queue[index].p) {
			if (self.admin_comp_queue[index].status == 0) {
				println("success");
			}
			println("completed sub queue ", self.admin_comp_queue[index].sqid, " entry ", self.admin_comp_queue[index].sqhd - 1, " entry");
			index += 1;
		}

		auto doorbell = cast<volatile u32*>(self.base + COMPLETION_Q_HEAD_DOORBELL(0, self.doorbell_stride));
		*doorbell = index % admin_comp_queue_size;

		arch_eoi();
	}

	bool init(Pci::Header0* hdr, usize base_addr) {
		base = base_addr;
		regs = cast<volatile Regs*>(base);

		if (!(CAP_CSS(regs->cap) & CSS_NVM)) {
			println("[nvme] controller doesn't support the nvm command set");
			return false;
		}

		auto min_pg_size = pow2(12 + CAP_MPSMIN(regs->cap));
		auto max_pg_size = pow2(12 + CAP_MPSMAX(regs->cap));

		if (PAGE_SIZE < min_pg_size || PAGE_SIZE > max_pg_size) {
			println("[nvme] controller doesn't support host page size");
			return false;
		}

		if (!nvme_vec) {
			nvme_vec = alloc_int_handler(irq, this);
			if (!nvme_vec) {
				panic("[nvme] failed to allocate interrupt");
			}
			hdr->set_irq(0, nvme_vec);
		}

		regs->cc &= ~CC_EN_MASK;

		while (CSTS_RDY(regs->csts)) {
			arch_spinloop_hint();
		}

		admin_sub_queue = cast<Cmd*>(new u8[0x1000]());
		admin_comp_queue = cast<CompletionEntry*>(new u8[0x1000]());

		regs->aqa = AQA_ASQS_V(admin_sub_queue_size) | AQA_ACQS_V(admin_comp_queue_size);

		regs->asq = VirtAddr {admin_sub_queue}.to_phys().as_usize();
		regs->acq = VirtAddr {admin_comp_queue}.to_phys().as_usize();

		auto supported_css = CAP_CSS(regs->cap);
		u32 css = 0;
		if (supported_css & CSS_NO_IO) {
			css = CSS_ADMIN_ONLY;
		}
		else if (supported_css & CSS_IO) {
			css = CSS_ALL_IO;
		}

		regs->cc = CC_MPS_V(log2(PAGE_SIZE) - 12) | CC_AMS_V(AMS_RR) | CC_CSS_V(css) |
			  CC_IOSQEC_V(log2(64)) | CC_IOCQES_V(log2(16)) | CC_EN_V(1);

		while (!CSTS_RDY(regs->csts)) {
			__builtin_ia32_pause();
		}

		println("controller ready");

		auto stride = pow2(CAP_DSTRD(regs->cap) + 2);
		println("doorbell stride: ", stride);
		doorbell_stride = stride;

		auto data = new u8[0x1000]();

		Cmd identify_cmd {
			.op = Op::Identify,
		};
		identify_cmd.identify.cns = Cns::Controller;
		identify_cmd.identify.dtpr[0] = VirtAddr {data}.to_phys().as_usize();
		identify_cmd.identify.mtpr = 0;

		*admin_sub_queue = identify_cmd;
		admin_sub_queue_i += 1;

		hdr->enable_irqs();

		auto doorbell = cast<volatile u32*>(base + SUBMISSION_Q_TAIL_DOORBELL(0, stride));
		auto comp_doorbell = cast<volatile u32*>(base + COMPLETION_Q_HEAD_DOORBELL(0, stride));
		admin_sub_queue_doorbell = doorbell;
		admin_comp_queue_doorbell = comp_doorbell;

		*doorbell = 1;

		return true;
	}
private:
	volatile Regs* regs {};
	usize base {};
	static constexpr usize admin_sub_queue_size = PAGE_SIZE / sizeof(Cmd);
	static constexpr usize admin_comp_queue_size = PAGE_SIZE / sizeof(CompletionEntry);
	Cmd* admin_sub_queue {};
	volatile CompletionEntry* admin_comp_queue {};
	u16 admin_sub_queue_i {};
	u16 admin_comp_queue_i {};
	usize doorbell_stride {};
	volatile u32* admin_sub_queue_doorbell {};
	volatile u32* admin_comp_queue_doorbell {};
};

void init_nvme(Pci::Header0* hdr) {
	println("initializing nvme controller");
	if (hdr->is_io_space(0)) {
		println("io space bar is not supported for NVMe");
		return;
	}

	println("bar size: ", hdr->get_bar_size(0));
	auto addr = hdr->map_bar(0);
	println("mapped bar, resulting phys addr is ", (void*) addr);

	hdr->common.command |= Pci::Cmd::BusMaster | Pci::Cmd::MemSpace;
	hdr->common.command &= ~Pci::Cmd::InterruptDisable;

	auto* power = cast<Pci::PowerCap*>(hdr->get_cap(Pci::Cap::PowerManagement));
	if (power) {
		println("setting power state");
	}

	auto controller = new NvmeController();
	auto result = controller->init(hdr, PhysAddr {addr}.to_virt().as_usize());
	if (!result) {
		ALLOCATOR.dealloc(controller, sizeof(*controller));
	}
}

static PciDriver pci_driver {
	.match = PciDriver::MATCH_CLASS | PciDriver::MATCH_SUBCLASS | PciDriver::MATCH_PROG,
	.dev_class = 0x1,
	.dev_subclass = 0x8,
	.dev_prog = 0x2,
	.load = init_nvme
};

PCI_DRIVER(nvme, pci_driver);