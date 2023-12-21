#ifndef CRESCENT_DEV_H
#define CRESCENT_DEV_H
#include "handle.h"

#define DEV_NAME_MAX 128

typedef enum {
	DEVICE_TYPE_FB,
	DEVICE_TYPE_SND,
	DEVICE_TYPE_PARTITION,
	DEVICE_TYPE_MAX
} CrescentDeviceType;

typedef struct {
	char name[DEV_NAME_MAX];
	CrescentHandle handle;
} CrescentDeviceInfo;

#endif