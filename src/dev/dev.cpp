#include "dev.hpp"

ManuallyDestroy<Spinlock<kstd::vector<kstd::shared_ptr<Device>>>> DEVICES
	[static_cast<int>(CrescentDeviceType::Max)] {};

usize dev_add(kstd::shared_ptr<Device> device, CrescentDeviceType type) {
	auto guard = DEVICES[static_cast<int>(type)]->lock();
	auto index = guard->size();
	guard->push(std::move(device));
	return index;
}

void dev_remove(usize index, CrescentDeviceType type) {
	auto guard = DEVICES[static_cast<int>(type)]->lock();
	guard->remove(index);
}
