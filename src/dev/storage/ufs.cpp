#include "ufs.hpp"
#include "mem/register.hpp"
#include "mem/vspace.hpp"
#include "sched/process.hpp"
#include "utils/driver.hpp"

namespace regs {
	static constexpr BitRegister<u32> CAP {0};
	static constexpr BitRegister<u32> VER {0x8};
	static constexpr BitRegister<u32> HCDDID {0x10};
	static constexpr BitRegister<u32> HCPMID {0x14};
	static constexpr BitRegister<u32> AHIT {0x18};
	static constexpr BitRegister<u32> IS {0x20};
	static constexpr BitRegister<u32> IE {0x24};
	static constexpr BitRegister<u32> HCS {0x30};
	static constexpr BitRegister<u32> HCE {0x34};
	static constexpr BasicRegister<u32> UTRLBA {0x50};
	static constexpr BasicRegister<u32> UTRLBAU {0x54};
	static constexpr BitRegister<u32> UTRLDBR {0x58};
	static constexpr BitRegister<u32> UTRLCLR {0x5C};
	static constexpr BitRegister<u32> UTRLRSR {0x60};
	static constexpr BitRegister<u32> UTRLCNR {0x64};
	static constexpr BasicRegister<u32> UTMRLBA {0x70};
	static constexpr BasicRegister<u32> UTMRLBAU {0x74};
	static constexpr BitRegister<u32> UTMRLDBR {0x78};
	static constexpr BitRegister<u32> UTMRLCLR {0x7C};
	static constexpr BitRegister<u32> UTMRLRSR {0x80};
	static constexpr BitRegister<u32> UICCMD {0x90};
	static constexpr BitRegister<u32> UCMDARG1 {0x94};
	static constexpr BitRegister<u32> UCMDARG2 {0x98};
	static constexpr BitRegister<u32> UCMDARG3 {0x9C};
}

namespace cap {
	static constexpr BitField<u32, u8> NUTRS {0, 5};
	static constexpr BitField<u32, u8> NORTT {8, 8};
	static constexpr BitField<u32, u8> NUTMRS {16, 3};
	static constexpr BitField<u32, bool> AS64 {24, 1};
}

namespace ver {
	static constexpr BitField<u32, u8> VS {0, 4};
	static constexpr BitField<u32, u8> MNR {4, 4};
	static constexpr BitField<u32, u8> MJR {8, 8};
}

namespace is {
	static constexpr BitField<u32, bool> ULSS {8, 1};
	static constexpr BitField<u32, bool> UCCS {10, 1};
}

namespace hcs {
	static constexpr BitField<u32, bool> DP {0, 1};
}

namespace hce {
	static constexpr BitField<u32, bool> HCE {0, 1};
	static constexpr BitField<u32, bool> CGE {1, 1};
}

namespace utrlrsr {
	static constexpr BitField<u32, bool> RUN {0, 1};
}

namespace utmrlrsr {
	static constexpr BitField<u32, bool> RUN {0, 1};
}

namespace uiccmd {
	static constexpr BitField<u32, u8> CMDOP {0, 8};
}

namespace uiccmdarg2 {
	static constexpr BitField<u32, u8> GENERIC_ERROR {0, 8};
}

namespace cmd {
	static constexpr u8 DME_GET = 1;
	static constexpr u8 DME_SET = 2;
	static constexpr u8 DME_PEER_GET = 3;
	static constexpr u8 DME_PEER_SET = 4;
	static constexpr u8 DME_POWERON = 0x10;
	static constexpr u8 DME_POWEROFF = 0x11;
	static constexpr u8 DME_ENABLE = 0x12;
	static constexpr u8 DME_RESET = 0x14;
	static constexpr u8 DME_ENDPOINTRESET = 0x15;
	static constexpr u8 DME_LINKSTARTUP = 0x16;
	static constexpr u8 DME_HIBERNAME_ENTER = 0x17;
	static constexpr u8 DME_HIBERNAME_EXIT = 0x18;
}

namespace error {
	static constexpr u8 SUCCESS = 0;
	static constexpr u8 FAILURE = 1;
}

struct TaskRequestDesc {
	u32 dw0;
	u32 dw1;
	u32 dw2;
	u32 dw3;

	struct {
		u32 dw4;
		u32 dw5;
		u32 dw6;
		u32 dw7;
		u32 dw8;
		u32 dw9;
		u32 dw10;
		u32 dw11;
	} request;

	struct {
		u32 dw12;
		u32 dw13;
		u32 dw14;
		u32 dw15;
		u32 dw16;
		u32 dw17;
		u32 dw18;
		u32 dw19;
	} response;
};

struct TransferRequest {
	u32 dw0;
	u32 dw1;
	u32 dw2;
	u32 dw3;
	u32 dw4;
	u32 dw5;
	u32 dw6;
	u32 dw7;
};

struct UfsController {
	explicit UfsController(IoSpace space) : space {space} {
		auto cap = space.load(regs::CAP);
		assert(cap & cap::AS64);

		auto ver = space.load(regs::VER);

		auto major = ((ver & ver::MJR) >> 4) * 10 + (ver & ver::MJR & 0xF);
		println("[kernel][ufs]: version ", major, ".", ver & ver::MNR, ".", ver & ver::VS);

		space.store(regs::HCE, 0);
		while (space.load(regs::HCE) & hce::HCE);

		space.store(regs::HCE, hce::HCE(true));
		while (!(space.load(regs::HCE) & hce::HCE));

		while (true) {
			space.store(regs::UICCMD, uiccmd::CMDOP(cmd::DME_LINKSTARTUP));
			while (!(space.load(regs::IS) & is::UCCS));

			space.store(regs::IS, is::UCCS(true));

			auto status = space.load(regs::UCMDARG2) & uiccmdarg2::GENERIC_ERROR;
			if (status == error::SUCCESS) {
				auto hcs = space.load(regs::HCS);
				if (hcs & hcs::DP) {
					break;
				}
			}

			while (!(space.load(regs::IS) & is::ULSS));
		}

		auto task_req_virt = KERNEL_VSPACE.alloc(PAGE_SIZE);
		assert(task_req_virt);

		auto task_req_phys = pmalloc(1);
		assert(task_req_phys);
		auto status = KERNEL_PROCESS->page_map.map(
			reinterpret_cast<u64>(task_req_virt),
			task_req_phys,
			PageFlags::Read | PageFlags::Write,
			CacheMode::Uncached);
		assert(status);

		memset(task_req_virt, 0, PAGE_SIZE);

		task_req_count = (cap & cap::NUTMRS) + 1;
		for (u32 i = 0; i < task_req_count; ++i) {
			task_reqs[i] = offset(task_req_virt, TaskRequestDesc*, i * sizeof(TaskRequestDesc));
		}

		space.store(regs::UTMRLBA, task_req_phys);
		space.store(regs::UTMRLBAU, task_req_phys >> 32);

		transfer_req_count = (cap & cap::NUTRS) + 1;
		for (u32 i = 0; i < transfer_req_count; ++i) {
			transfer_reqs[i] = offset(task_req_virt, TransferRequest*, PAGE_SIZE / 2 + i * sizeof(TaskRequestDesc));
		}

		usize transfer_req_phys = task_req_phys + PAGE_SIZE / 2;
		space.store(regs::UTRLBA, transfer_req_phys);
		space.store(regs::UTRLBAU, transfer_req_phys >> 32);

		space.store(regs::UTMRLRSR, utmrlrsr::RUN(true));
		space.store(regs::UTRLRSR, utrlrsr::RUN(true));
	}

	IoSpace space;
	volatile TaskRequestDesc* task_reqs[8] {};
	volatile TransferRequest* transfer_reqs[32] {};
	u32 task_req_count = 0;
	u32 transfer_req_count = 0;
};

InitStatus ufs_init(IoSpace space) {
	new UfsController {space};
	return InitStatus::Success;
}
