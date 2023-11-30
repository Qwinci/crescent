#include "arch/mod.h"
#include "arch/x86/dev/serial.h"
#include "assert.h"
#include "dev/fb.h"
#include "dev/fbcon.h"
#include "limine/limine.h"
#include "mem/allocator.h"
#include "mem/utils.h"
#include "string.h"

static volatile struct limine_framebuffer_request FB_REQUEST = {
	.id = LIMINE_FRAMEBUFFER_REQUEST
};

static FbDev x86_boot_fb;

void x86_init_con() {
	if (FB_REQUEST.response && FB_REQUEST.response->framebuffer_count) {
		struct limine_framebuffer* limine_fb = FB_REQUEST.response->framebuffers[0];
		memcpy(x86_boot_fb.generic.name, "fb#0", sizeof("fb#0"));
		x86_boot_fb.base = limine_fb->address;
		x86_boot_fb.phys_base = to_phys(limine_fb->address);
		x86_boot_fb.info.width = limine_fb->width;
		x86_boot_fb.info.height = limine_fb->height;
		x86_boot_fb.info.pitch = limine_fb->pitch;
		x86_boot_fb.info.bpp = limine_fb->bpp;
		x86_boot_fb.info.fmt = SYS_FB_FORMAT_BGRA32;

		Module font_module = arch_get_module("Tamsyn8x16r.psf");
		if (font_module.base) {
			fbcon_init(&x86_boot_fb, font_module.base);
		}
	}

	serial_attach_log(0x3F8);
}

void x86_init_fbs() {
	if (FB_REQUEST.response) {
		for (usize i = 0; i < FB_REQUEST.response->framebuffer_count; ++i) {
			struct limine_framebuffer* limine_fb = FB_REQUEST.response->framebuffers[i];
			FbDev* fb = kcalloc(sizeof(FbDev));
			assert(fb);

			snprintf(fb->generic.name, sizeof(fb->generic.name), "fb#%d", (int) i);

			fb->base = limine_fb->address;
			fb->phys_base = to_phys(limine_fb->address);
			fb->info.width = limine_fb->width;
			fb->info.height = limine_fb->height;
			fb->info.pitch = limine_fb->pitch;
			fb->info.bpp = limine_fb->bpp;
			fb->info.fmt = SYS_FB_FORMAT_BGRA32;
			dev_add(&fb->generic, DEVICE_TYPE_FB);
		}
	}
}
