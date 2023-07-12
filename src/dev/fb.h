#pragma once
#include "types.h"
#include "crescent/fb.h"

void fb_set_pixel(SysFramebuffer* self, usize x, usize y, u32 color);
u32 fb_get_pixel(SysFramebuffer* self, usize x, usize y);
void fb_clear(SysFramebuffer* self, u32 color);

extern SysFramebuffer* primary_fb;
