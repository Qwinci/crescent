#include "dev/fb.h"
#include "dev/fbcon.h"
#include "limine/limine.h"
#include "arch/mod.h"
#include "mem/allocator.h"
#include "assert.h"

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

		Module font_module = arch_get_module("Tamsyn8x16r.psf");
		if (font_module.base) {
			fbcon_init(primary_fb, font_module.base);
		}
	}
}

void x86_init_fbs() {
	if (FB_REQUEST.response) {
		for (usize i = 0; i < FB_REQUEST.response->framebuffer_count; ++i) {
			struct limine_framebuffer* limine_fb = FB_REQUEST.response->framebuffers[i];
			LinkedSysFramebuffer* fb = kmalloc(sizeof(LinkedSysFramebuffer));
			assert(fb);
			fb->next = NULL;

			fb->fb.base = limine_fb->address;
			fb->fb.width = limine_fb->width;
			fb->fb.height = limine_fb->height;
			fb->fb.pitch = limine_fb->pitch;
			fb->fb.bpp = limine_fb->bpp;
			fb->fb.fmt = SYS_FB_FORMAT_BGRA32;
			fb->fb.primary = i == 0;
			sys_fb_add(fb);
		}
	}
}
