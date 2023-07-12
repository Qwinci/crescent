#include "fb.h"
#include "assert.h"

void fb_set_pixel(SysFramebuffer* self, usize x, usize y, u32 color) {
	assert(self);
	assert(self->base);
	assert(x < self->width);
	assert(y < self->height);
	if (self->fmt == SYS_FB_FORMAT_BGRA32) {
		*offset(self->base, u32*, y * self->pitch + x * (self->bpp / 8)) = color;
	}
	else {
		return;
	}
}

u32 fb_get_pixel(SysFramebuffer* self, usize x, usize y) {
	if (self->fmt == SYS_FB_FORMAT_BGRA32) {
		return *offset(self->base, u32*, y * self->pitch + x * (self->bpp / 8));
	}
	else {
		return 0;
	}
}

void fb_clear(SysFramebuffer* self, u32 color) {
	for (usize y = 0; y < self->height; ++y) {
		for (usize x = 0; x < self->width; ++x) {
			fb_set_pixel(self, x, y, color);
		}
	}
}

SysFramebuffer* primary_fb = NULL;