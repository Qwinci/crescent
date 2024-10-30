#include "fw_cfg_aarch64.hpp"
#include "mem/iospace.hpp"
#include "mem/vspace.hpp"
#include "sched/process.hpp"
#include "utils/driver.hpp"

namespace {
	IoSpace SPACE {};
	usize DMA_PAGE_PHYS {};
	void* DMA_PAGE {};
}

namespace regs {
	static constexpr BasicRegister<u64> DATA {0};
	static constexpr BasicRegister<u16> SELECTOR {8};
	static constexpr BasicRegister<u64> DMA_BASE {16};
}

namespace sel {
	static constexpr u16 FILE_DIR = 0x19;
}

struct FwCfgDmaAccess {
	u32 control;
	u32 length;
	u64 address;
};

struct FwCfgFile {
	u32 size;
	u16 select;
	u16 reserved;
	char name[56];
};

namespace {
	constexpr u32 CONTROL_ERROR = 1 << 0;
	constexpr u32 CONTROL_READ = 1 << 1;
	constexpr u32 CONTROL_SELECT = 1 << 3;
	constexpr u32 CONTROL_WRITE = 1 << 4;
}

static InitStatus fw_cfg_init(DtbNode& node) {
	assert(!node.regs.is_empty());
	auto reg = node.regs[0];

	SPACE = {reg.addr, reg.size};
	assert(SPACE.map(CacheMode::Uncached));

	DMA_PAGE = KERNEL_VSPACE.alloc_backed(PAGE_SIZE, PageFlags::Read | PageFlags::Write, CacheMode::Uncached);
	assert(DMA_PAGE);
	DMA_PAGE_PHYS = KERNEL_PROCESS->page_map.get_phys(reinterpret_cast<u64>(DMA_PAGE));
	assert(DMA_PAGE_PHYS);

	return InitStatus::Success;
}

static bool qemu_fw_cfg_read_control(u32 control, void* data, u32 size) {
	assert(size < PAGE_SIZE - sizeof(FwCfgDmaAccess));

	auto* cfg = static_cast<volatile FwCfgDmaAccess*>(DMA_PAGE);
	cfg->control = kstd::to_be(control);
	cfg->length = kstd::to_be(size);
	cfg->address = kstd::to_be<u64>(DMA_PAGE_PHYS + sizeof(FwCfgDmaAccess));

	SPACE.store(regs::DMA_BASE, kstd::to_be(DMA_PAGE_PHYS));
	while (kstd::to_ne_from_be(cfg->control) & ~CONTROL_ERROR);

	if (!kstd::to_ne_from_be(cfg->control)) {
		memcpy(data, const_cast<FwCfgDmaAccess*>(&cfg[1]), size);
		return true;
	}
	else {
		return false;
	}
}

static bool qemu_fw_cfg_read_select(u16 selector, void* data, u32 size) {
	u32 control = CONTROL_SELECT | CONTROL_READ | selector << 16;
	return qemu_fw_cfg_read_control(control, data, size);
}

static bool qemu_fw_cfg_read(void* data, u32 size) {
	return qemu_fw_cfg_read_control(CONTROL_READ, data, size);
}

kstd::optional<u16> qemu_fw_cfg_get_file(kstd::string_view name) {
	if (!DMA_PAGE_PHYS) {
		return {};
	}

	u32 num_files;
	assert(qemu_fw_cfg_read_select(sel::FILE_DIR, &num_files, 4));
	num_files = kstd::to_ne_from_be(num_files);
	for (u32 i = 0; i < num_files; ++i) {
		FwCfgFile file {};
		assert(qemu_fw_cfg_read(&file, sizeof(file)));

		if (file.name == name) {
			return kstd::to_ne_from_be(file.select);
		}
	}

	return {};
}

bool qemu_fw_cfg_write(u16 selector, const void* data, u32 size) {
	assert(size < PAGE_SIZE - sizeof(FwCfgDmaAccess));

	auto* cfg = static_cast<volatile FwCfgDmaAccess*>(DMA_PAGE);
	u32 control = CONTROL_SELECT | CONTROL_WRITE | selector << 16;
	cfg->control = kstd::to_be(control);
	cfg->length = kstd::to_be(size);
	cfg->address = kstd::to_be<u64>(DMA_PAGE_PHYS + sizeof(FwCfgDmaAccess));
	memcpy(const_cast<FwCfgDmaAccess*>(&cfg[1]), data, size);

	SPACE.store(regs::DMA_BASE, kstd::to_be(DMA_PAGE_PHYS));
	while (kstd::to_ne_from_be(cfg->control) & ~CONTROL_ERROR);
	return !kstd::to_ne_from_be(cfg->control);
}

static constexpr DtDriver FW_CFG_DRIVER {
	.name = "Qemu fw_cfg driver",
	.init = fw_cfg_init,
	.compatible {
		"qemu,fw-cfg-mmio"
	},
	.provides {
		"fw_cfg"
	}
};

DT_DRIVER(FW_CFG_DRIVER);
