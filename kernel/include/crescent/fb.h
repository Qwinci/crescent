#ifndef CRESCENT_FB_H
#define CRESCENT_FB_H
#include <stddef.h>

typedef enum {
	SYS_FB_FORMAT_BGRA32
} CrescentFramebufferFormat;

typedef struct SysFramebufferInfo {
	size_t width;
	size_t height;
	size_t pitch;
	size_t bpp;
	CrescentFramebufferFormat fmt;
} CrescentFramebufferInfo;

typedef enum {
	DEVMSG_FB_INFO,
	DEVMSG_FB_MAP
} CrescentDevMsgFb;

#endif
