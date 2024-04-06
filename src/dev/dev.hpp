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

	virtual int handle_request(const kstd::vector<u8>& data, kstd::vector<u8>& res, usize max_size) = 0;
};

usize dev_add(kstd::shared_ptr<Device> device, CrescentDeviceType type);
void dev_remove(usize index, CrescentDeviceType type);

extern ManuallyDestroy<Spinlock<kstd::vector<kstd::shared_ptr<Device>>>> DEVICES
	[static_cast<int>(CrescentDeviceType::Max)];
