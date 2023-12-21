#pragma once
#include "utils/handle.h"
#include "arch/misc.h"
#include "crescent/dev.h"

int sys_devmsg(CrescentHandle handle, size_t msg, __user void* data);
int sys_devenum(CrescentDeviceType type, __user CrescentDeviceInfo* res, __user size_t* count);

typedef struct {
	CrescentDeviceType type;
	usize refcount;
	char name[128];
} GenericDevice;

void dev_add(GenericDevice* device, CrescentDeviceType type);
void dev_remove(GenericDevice* device, CrescentDeviceType type);
GenericDevice* dev_get(const char* name, usize name_len);

typedef struct {
	GenericDevice** devices;
	Mutex lock;
	usize len;
	usize cap;
} DeviceList;

extern DeviceList DEVICES[DEVICE_TYPE_MAX];
