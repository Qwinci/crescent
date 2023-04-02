#pragma once
#include "types.h"

typedef enum {
	FB_FMT_BGRA32
} FramebufferFormat;

typedef struct Framebuffer {
	u8* base;
	usize width;
	usize height;
	usize pitch;
	usize bpp;
	FramebufferFormat fmt;
} Framebuffer;

void fb_set_pixel(Framebuffer* self, usize x, usize y, u32 color);
u32 fb_get_pixel(Framebuffer* self, usize x, usize y);
void fb_clear(Framebuffer* self, u32 color);

extern Framebuffer* primary_fb;