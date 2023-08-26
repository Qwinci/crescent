#pragma once
#include "utils/handle.h"
#include "arch/misc.h"
#include "crescent/dev.h"

int sys_devmsg(Handle handle, size_t msg, __user void* data);
int sys_devenum(DeviceType type, __user Handle* res, __user size_t* count);

typedef struct {
	DeviceType type;
	usize refcount;
	char name[64];
} GenericDevice;

void dev_add(GenericDevice* device, DeviceType type);
void dev_remove(GenericDevice* device, DeviceType type);

typedef struct {
	GenericDevice** devices;
	Mutex lock;
	usize len;
	usize cap;
} DeviceList;

extern DeviceList DEVICES[DEVICE_TYPE_MAX];
