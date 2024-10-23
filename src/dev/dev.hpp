#pragma once
#include "vector.hpp"
#include "manually_destroy.hpp"
#include "shared_ptr.hpp"
#include "utils/spinlock.hpp"
#include "string.hpp"
#include "crescent/devlink.h"

struct Device {
	virtual ~Device() = default;

	kstd::string name {};
	kstd::atomic<usize> open_count {};
	bool exclusive {};

	virtual int handle_request(const kstd::vector<u8>& data, kstd::vector<u8>& res, usize max_size) = 0;
};

struct DeviceHandle {
	explicit DeviceHandle(kstd::shared_ptr<Device> device) : device {std::move(device)} {
		this->device->open_count.fetch_add(1, kstd::memory_order::relaxed);
	}

	~DeviceHandle() {
		device->open_count.fetch_sub(1, kstd::memory_order::relaxed);
	}

	kstd::shared_ptr<Device> device;
};

usize dev_add(kstd::shared_ptr<Device> device, CrescentDeviceType type);
void dev_remove(usize index, CrescentDeviceType type);

extern ManuallyDestroy<Spinlock<kstd::vector<kstd::shared_ptr<Device>>>> DEVICES
	[static_cast<int>(CrescentDeviceType::Max)];
