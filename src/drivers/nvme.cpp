#include "console.hpp"
#include "pci.hpp"
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
#define CC_IOSQES(r) (r >> 16 & 0b1111)
#define CC_IOSQES_MASK 0b1111
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

struct NvmeController {
	bool init(Pci::Header0* hdr, usize base_addr) {
		// todo
		return false;

		base = base_addr;

		// todo check version

		auto cc = reg32(REG32_CC);
		auto cap = reg64(REG64_CAP);
		auto csts = reg32(REG32_CSTS);

		if (!(CAP_CSS(*cap) & CSS_NVM)) {
			println("[nvme] controller doesn't support the nvm command set");
			return false;
		}

		auto min_pg_size = pow2(12 + CAP_MPSMIN(*cap));
		auto max_pg_size = pow2(12 + CAP_MPSMAX(*cap));

		if (PAGE_SIZE < min_pg_size || PAGE_SIZE > max_pg_size) {
			println("[nvme] controller doesn't support host page size");
			return false;
		}

		*cc &= ~CC_EN_MASK;

		while (CSTS_RDY(*csts)) {
			__builtin_ia32_pause();
		}

		auto admin_sub_queue = new u8[0x1000];
		auto admin_comp_queue = new u8[0x1000];

		auto aqa = reg32(REG32_AQA);
		*aqa = AQA_ASQS_V(0x1000 / 64) | AQA_ACQS_V(0x1000 / 16);

		auto asq = reg64(REG64_ASQ);
		auto acq = reg64(REG64_ACQ);

		*asq = VirtAddr {admin_sub_queue}.to_phys().as_usize();
		*acq = VirtAddr {admin_comp_queue}.to_phys().as_usize();

		auto supported_css = CAP_CSS(*cap);
		u32 css = 0;
		if (supported_css & CSS_NO_IO) {
			css = CSS_ADMIN_ONLY;
		}
		else if (supported_css & CSS_IO) {
			css = CSS_ALL_IO;
		}

		*cc = CC_MPS_V(log2(PAGE_SIZE) - 12) | CC_AMS_V(AMS_RR) | CC_CSS_V(css);

		println("csts: ", *csts);

		*cc |= CC_EN_V(1);

		println("csts: ", *csts);

		while (!CSTS_RDY(*csts)) {
			__builtin_ia32_pause();
		}

		println("controller ready");

		return true;
	}

	[[nodiscard]] volatile u32* reg32(u32 reg) const {
		return cast<volatile u32*>(base + reg);
	}

	[[nodiscard]] volatile u64* reg64(u32 reg) const {
		return cast<volatile u64*>(base + reg);
	}
private:
	usize base;
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
		delete controller;
	}
}