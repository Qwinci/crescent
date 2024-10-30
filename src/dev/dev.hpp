#pragma once
#include "double_list.hpp"
#include "utils/spinlock.hpp"

struct Device {
	virtual ~Device() = default;

	virtual int suspend() = 0;
	virtual int resume() = 0;

	DoubleListHook hook {};
};

extern IrqSpinlock<DoubleList<Device, &Device::hook>> ALL_DEVICES;

void dev_add(Device* device);
void dev_remove(Device* device);
void dev_suspend_all();
void dev_resume_all();
