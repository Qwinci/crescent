#include "console.hpp"
#include "pci.hpp"

#define SUBMISSION_Q_TAIL_DOORBELL(x, stride) (0x1000 + 2 * (x) * (4 << (stride)))
#define COMPLETION_Q_HEAD_DOORBELL(x, stride) (0x1000 + (2 * (x) + 1) * (4 << (stride)))

/// Controller Capabilities
#define REG64_CAP 0
/// Version
#define REG32_VS 0x8
/// Interrupt Mask Set
#define REG32_INTMS 0xC
/// Interrupt Mask Clear
#define REG32_INTMC 0xF
/// Controller Configuration
#define REG32_CC 0x14
/// Controller Status
#define REG32_CSTS 0x1C
/// NVM Subsystem Reset
#define REG32_NSSR 0x20
/// Admin Queue Attributes
#define REG32_AQA 0x24
/// Admin Submission Queue Base
#define REG64_ASQ 0x28
/// Admin Completion Queue Base
#define REG64_ACQ 0x30

struct Cap {
	u16 max_queue_entr;
	u16 cont_queues : 1;
	u16 arbit_support : 2;
private:
	u16 reserved1 : 5;
public:
	u16 timeout : 8;
	u32 doorbell_stride : 4;
	u32 nvm_reset_support : 1;
	u32 cmd_set_support : 8;
	u32 boot_part_support : 1;
	u32 controller_pwr_scope : 2;
	u32 mem_pg_size_min : 4;
	u32 mem_pg_size_max : 4;
	u32 persistent_mem_support : 1;
	u32 controller_mem_buf_support : 1;
	u32 nvm_shutdown_support : 1;
	u32 controller_ready_modes : 2;
private:
	u32 reserved2 : 3;
};
static_assert(sizeof(Cap) == 8);

struct Version {
private:
	u8 reserved;
public:
	u8 minor;
	u16 major;
};
static_assert(sizeof(Version) == 4);

struct Regs {
	volatile Cap cap;
	volatile Version vs;
	volatile u32 intms;
	volatile u32 intmc;
	volatile u32 cc;
	volatile u32 csts;
	volatile u32 nssr;
	volatile u32 aqa;
	volatile u64 asq;
	volatile u64 acq;
};

struct NvmeController {
	void init(usize base_addr) {
		base = base_addr;
		regs = cast<Regs*>(base);

		println("max pg size: ", regs->cap.mem_pg_size_max);
	}

private:
	Regs* regs;
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
	controller->init(PhysAddr {addr}.to_virt().as_usize());
}