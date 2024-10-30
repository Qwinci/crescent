#include "dev.hpp"

void dev_add(Device* device) {
	ALL_DEVICES.lock()->push(device);
}

void dev_remove(Device* device) {
	ALL_DEVICES.lock()->remove(device);
}

void dev_suspend_all() {
	auto guard = ALL_DEVICES.lock();
	for (auto& device : *guard) {
		device.suspend();
	}
}

void dev_resume_all() {
	auto guard = ALL_DEVICES.lock();
	for (auto& device : *guard) {
		device.resume();
	}
}

constinit IrqSpinlock<DoubleList<Device, &Device::hook>> ALL_DEVICES;
