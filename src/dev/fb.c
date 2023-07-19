#include "fb.h"
#include "assert.h"
#include "sched/mutex.h"
#include "string.h"

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

static Mutex SYS_FB_LOCK = {};
static LinkedSysFramebuffer* SYS_FBS = NULL;
static usize SYS_FB_COUNT = 0;

void sys_fb_add(LinkedSysFramebuffer* fb) {
	mutex_lock(&SYS_FB_LOCK);
	fb->next = SYS_FBS;
	SYS_FBS = fb;
	SYS_FB_COUNT += 1;
	mutex_unlock(&SYS_FB_LOCK);
}

void sys_fb_get(SysFramebuffer* res, usize* count) {
	mutex_lock(&SYS_FB_LOCK);

	if (count) {
		*count = SYS_FB_COUNT;
	}
	if (res) {
		usize i = 0;
		for (LinkedSysFramebuffer* fb = SYS_FBS; fb; fb = fb->next) {
			memcpy(res + i, &fb->fb, sizeof(SysFramebuffer));
			i += 1;
		}
	}

	mutex_unlock(&SYS_FB_LOCK);
}

SysFramebuffer* primary_fb = NULL;

