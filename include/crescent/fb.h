#ifndef CRESCENT_FB_H
#define CRESCENT_FB_H
#include <stddef.h>

typedef enum {
	SYS_FB_FORMAT_BGRA32
} SysFramebufferFormat;

typedef struct SysFramebuffer {
	void* base;
	size_t width;
	size_t height;
	size_t pitch;
	size_t bpp;
	SysFramebufferFormat fmt;
	bool primary;
} SysFramebuffer;

#endif
