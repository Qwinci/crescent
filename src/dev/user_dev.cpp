#include "format.hpp"
#include "user_dev.hpp"

ManuallyDestroy<Spinlock<kstd::vector<kstd::shared_ptr<UserDevice>>>> USER_DEVICES
	[static_cast<int>(CrescentDeviceType::Max)] {};

usize user_dev_add(kstd::shared_ptr<UserDevice> device, CrescentDeviceType type) {
	auto guard = USER_DEVICES[static_cast<int>(type)]->lock();
	auto index = guard->size();

	kstd::formatter fmt {device->name};
	fmt << index;

	guard->push(std::move(device));
	return index;
}

void user_dev_remove(usize index, CrescentDeviceType type) {
	auto guard = USER_DEVICES[static_cast<int>(type)]->lock();
	guard->remove(index);
}
