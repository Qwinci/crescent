#pragma once
#include "types.hpp"
#include "mem/iospace.hpp"
#include "manually_init.hpp"
#include "stdio.hpp"

struct Framebuffer final : LogSink {
	Framebuffer(usize phys, u32 width, u32 height, usize pitch, u32 bpp);

	void write(kstd::string_view str) override;
	void set_color(Color color) override;
	void clear();

	usize phys;
	IoSpace space;
	usize pitch;
	u32 width;
	u32 height;
	u32 bpp;
	u32 fg_color {0x00FF00};
};

extern ManuallyInit<Framebuffer> BOOT_FB;
