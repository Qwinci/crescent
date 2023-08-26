#ifndef CRESCENT_FB_H
#define CRESCENT_FB_H
#include <stddef.h>

typedef enum {
	SYS_FB_FORMAT_BGRA32
} SysFramebufferFormat;

typedef struct SysFramebufferInfo {
	size_t width;
	size_t height;
	size_t pitch;
	size_t bpp;
	SysFramebufferFormat fmt;
} SysFramebufferInfo;

typedef enum {
	DEVMSG_FB_INFO,
	DEVMSG_FB_MAP
} DevMsgFb;

#endif
