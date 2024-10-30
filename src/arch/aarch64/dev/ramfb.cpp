#include "ramfb.hpp"
#include "arch/paging.hpp"
#include "bit.hpp"
#include "cstring.hpp"
#include "dev/fb/fb.hpp"
#include "dev/qemu/fw_cfg_aarch64.hpp"
#include "mem/mem.hpp"
#include "mem/pmalloc.hpp"

struct [[gnu::packed]] RamfbConfig {
	u64 addr;
	u32 fourcc;
	u32 flags;
	u32 width;
	u32 height;
	u32 stride;

	void serialize() {
		addr = kstd::to_be(addr);
		fourcc = kstd::to_be(fourcc);
		flags = kstd::to_be(flags);
		width = kstd::to_be(width);
		height = kstd::to_be(height);
		stride = kstd::to_be(stride);
	}
};

static constexpr u32 fourcc_code(char a, char b, char c, char d) {
	return a | b << 8 | c << 16 | d << 24;
}

static constexpr u32 DRM_FORMAT_XRGB8888 = fourcc_code('X', 'R', '2', '4');

void ramfb_init() {
	auto file_opt = qemu_fw_cfg_get_file("etc/ramfb");
	if (!file_opt) {
		return;
	}
	auto file = file_opt.value();

	u32 width = 1024;
	u32 height = 768;
	u32 stride = width * 4;

	auto addr = pmalloc((stride * height + PAGE_SIZE) / PAGE_SIZE);
	assert(addr);
	memset(to_virt<void>(addr), 0, stride * height);

	RamfbConfig config {
		.addr = addr,
		.fourcc = DRM_FORMAT_XRGB8888,
		.flags = 0,
		.width = width,
		.height = height,
		.stride = stride
	};
	config.serialize();

	assert(qemu_fw_cfg_write(file, &config, sizeof(config)));

	BOOT_FB.initialize(
		addr,
		width,
		height,
		stride,
		32U);
}
