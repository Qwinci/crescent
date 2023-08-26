#include "fb.h"
#include "assert.h"
#include "sched/mutex.h"
#include "string.h"

void fb_set_pixel(FbDev* self, usize x, usize y, u32 color) {
	assert(self);
	assert(self->base);
	assert(x < self->info.width);
	assert(y < self->info.height);
	if (self->info.fmt == SYS_FB_FORMAT_BGRA32) {
		*offset(self->base, u32*, y * self->info.pitch + x * (self->info.bpp / 8)) = color;
	}
	else {
		return;
	}
}

u32 fb_get_pixel(FbDev* self, usize x, usize y) {
	if (self->info.fmt == SYS_FB_FORMAT_BGRA32) {
		return *offset(self->base, u32*, y * self->info.pitch + x * (self->info.bpp / 8));
	}
	else {
		return 0;
	}
}

void fb_clear(FbDev* self, u32 color) {
	for (usize y = 0; y < self->info.height; ++y) {
		for (usize x = 0; x < self->info.width; ++x) {
			fb_set_pixel(self, x, y, color);
		}
	}
}
