#include "dev/fb/fb.hpp"
#include "gpu.hpp"
#include "gpu_dev.hpp"
#include "manually_destroy.hpp"
#include "manually_init.hpp"
#include "mem/portspace.hpp"
#include "mem/register.hpp"
#include "utils/driver.hpp"

namespace regs {
	static constexpr BasicRegister<u16> IOPORT_INDEX {0};
	static constexpr BasicRegister<u16> IOPORT_DATA {1};

	static constexpr u16 INDEX_ID = 0;
	static constexpr u16 INDEX_XRES = 1;
	static constexpr u16 INDEX_YRES = 2;
	static constexpr u16 INDEX_BPP = 3;
	static constexpr u16 INDEX_ENABLE = 4;
	static constexpr u16 INDEX_BANK = 5;
	static constexpr u16 INDEX_VIRT_WIDTH = 6;
	static constexpr u16 INDEX_VIRT_HEIGHT = 7;
	static constexpr u16 INDEX_X_OFFSET = 8;
	static constexpr u16 INDEX_Y_OFFSET = 9;
}

namespace {
	PortSpace SPACE {0x1CE};
}

static u16 vbe_read(u16 index) {
	SPACE.store(regs::IOPORT_INDEX, index);
	return SPACE.load(regs::IOPORT_DATA);
}

static void vbe_write(u16 index, u16 data) {
	SPACE.store(regs::IOPORT_INDEX, index);
	SPACE.store(regs::IOPORT_DATA, data);
}

struct VbeGpuSurface : public GpuSurface {
	constexpr VbeGpuSurface(usize vram_phys, u32 y_offset)
		: vram_phys {vram_phys}, y_offset {y_offset} {}

	usize get_phys() override {
		return vram_phys;
	}

	usize vram_phys;
	u32 y_offset;
};

struct VbeGpu : public Gpu {
	GpuSurface* create_surface() override {
		auto pitch = BOOT_FB->pitch;

		u32 offset = 0;
		if (!used_offsets.is_empty()) {
			offset = *used_offsets.back() + height * pitch;
		}
		if (offset + height * pitch > vram_size) {
			return nullptr;
		}
		used_offsets.push(offset);

		u32 y_offset = offset / pitch;
		return new VbeGpuSurface {vram_base + offset, y_offset};
	}

	void destroy_surface(GpuSurface* surface) override {
		for (usize i = 0; i < used_offsets.size(); ++i) {
			if (used_offsets[i] == static_cast<VbeGpuSurface*>(surface)->vram_phys) {
				used_offsets.remove(i);
				break;
			}
		}

		delete static_cast<VbeGpuSurface*>(surface);
	}

	void flip(GpuSurface* surface) override {
		auto vbe_surface = static_cast<VbeGpuSurface*>(surface);
		vbe_write(regs::INDEX_Y_OFFSET, vbe_surface->y_offset);
	}

	kstd::vector<u32> used_offsets;
	usize vram_base {};
	usize vram_size {};
	u16 width {};
	u16 height {};
};

namespace {
	ManuallyDestroy<VbeGpu> VBE_GPU {};
	ManuallyInit<kstd::shared_ptr<GpuDevice>> VBE_GPU_DEVICE {};
}

static bool bochs_vbe_init(pci::Device& device) {
	if (vbe_read(regs::INDEX_ID) != 0xB0C5) {
		return false;
	}

	auto width = vbe_read(regs::INDEX_XRES);
	auto height = vbe_read(regs::INDEX_YRES);
	VBE_GPU->width = width;
	VBE_GPU->height = height;
	VBE_GPU->owns_boot_fb = true;
	if (device.is_64bit(0)) {
		VBE_GPU->vram_base = (device.hdr0->bars[0] & ~0xF) | static_cast<u64>(device.hdr0->bars[1]) << 32;
	}
	else {
		VBE_GPU->vram_base = device.hdr0->bars[0] & ~0xF;
	}
	VBE_GPU->vram_size = device.get_bar_size(0);

	VBE_GPU_DEVICE.initialize(kstd::make_shared<GpuDevice>(&*VBE_GPU, "vbe gpu"));
	dev_add(*VBE_GPU_DEVICE, CrescentDeviceType::Gpu);

	return true;
}

static PciDriver VBE_DRIVER {
	.init = bochs_vbe_init,
	.match = PciMatch::Device,
	.devices {
		{.vendor = 0x1234, .device = 0x1111}
	}
};

PCI_DRIVER(VBE_DRIVER);