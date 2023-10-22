#pragma once
#include "utils/handle.h"
#include "arch/misc.h"
#include "crescent/dev.h"

int sys_devmsg(Handle handle, size_t msg, __user void* data);
int sys_devenum(DeviceType type, __user DeviceInfo* res, __user size_t* count);

typedef struct {
	DeviceType type;
	usize refcount;
	char name[128];
} GenericDevice;

void dev_add(GenericDevice* device, DeviceType type);
void dev_remove(GenericDevice* device, DeviceType type);
GenericDevice* dev_get(const char* name, usize name_len);

typedef struct {
	GenericDevice** devices;
	Mutex lock;
	usize len;
	usize cap;
} DeviceList;

extern DeviceList DEVICES[DEVICE_TYPE_MAX];
