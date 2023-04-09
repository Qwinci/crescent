#include "fb.h"

void fb_set_pixel(Framebuffer* self, usize x, usize y, u32 color) {
	if (self->fmt == FB_FMT_BGRA32) {
		*offset(self->base, u32*, y * self->pitch + x * self->bpp) = color;
	}
	else {
		return;
	}
}

u32 fb_get_pixel(Framebuffer* self, usize x, usize y) {
	if (self->fmt == FB_FMT_BGRA32) {
		return *offset(self->base, u32*, y * self->pitch + x * self->bpp);
	}
	else {
		return 0;
	}
}

void fb_clear(Framebuffer* self, u32 color) {
	for (usize y = 0; y < self->height; ++y) {
		for (usize x = 0; x < self->width; ++x) {
			fb_set_pixel(self, x, y, color);
		}
	}
}

Framebuffer* primary_fb = NULL;