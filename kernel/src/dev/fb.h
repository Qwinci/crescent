#pragma once
#include "types.h"
#include "crescent/fb.h"
#include "sys/dev.h"

typedef struct FbDev {
	GenericDevice generic;
	void* base;
	usize phys_base;
	SysFramebufferInfo info;
} FbDev;

int fbdev_devmsg(FbDev* self, DevMsgFb msg, __user void* data);

void fb_set_pixel(FbDev* self, usize x, usize y, u32 color);
u32 fb_get_pixel(FbDev* self, usize x, usize y);
void fb_clear(FbDev* self, u32 color);
