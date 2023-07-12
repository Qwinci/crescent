#include "dev/fb.h"
#include "dev/fbcon.h"
#include "limine/limine.h"
#include "mod.h"

static volatile struct limine_framebuffer_request FB_REQUEST = {
	.id = LIMINE_FRAMEBUFFER_REQUEST
};

static SysFramebuffer x86_boot_fb;

void x86_init_con() {
	if (FB_REQUEST.response && FB_REQUEST.response->framebuffer_count) {
		struct limine_framebuffer* limine_fb = FB_REQUEST.response->framebuffers[0];
		x86_boot_fb.base = limine_fb->address;
		x86_boot_fb.width = limine_fb->width;
		x86_boot_fb.height = limine_fb->height;
		x86_boot_fb.pitch = limine_fb->pitch;
		x86_boot_fb.bpp = limine_fb->bpp;
		x86_boot_fb.fmt = SYS_FB_FORMAT_BGRA32;
		x86_boot_fb.primary = true;
		primary_fb = &x86_boot_fb;

		Module font_module = x86_module_get("Tamsyn8x16r.psf");
		if (font_module.base) {
			fbcon_init(primary_fb, font_module.base);
		}
	}
}