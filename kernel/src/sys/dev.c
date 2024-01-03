#include "dev.h"
#include "arch/cpu.h"
#include "arch/executor.h"
#include "crescent/sys.h"
#include "dev/fb.h"
#include "fs.h"
#include "mem/allocator.h"
#include "mem/user.h"
#include "string.h"
#include "utils/math.h"

DeviceList DEVICES[DEVICE_TYPE_MAX] = {};

void dev_add(GenericDevice* device, CrescentDeviceType type) {
	DeviceList* list = &DEVICES[type];
	mutex_lock(&list->lock);

	device->type = type;

	if (list->len == list->cap) {
		usize new_cap = list->cap < 8 ? 8 : list->cap + list->cap / 2;
		list->devices = krealloc(list->devices, list->cap * sizeof(GenericDevice*), new_cap * sizeof(GenericDevice*));
		assert(list->devices);
		list->cap = new_cap;
	}

	list->devices[list->len++] = device;

	mutex_unlock(&list->lock);
}

GenericDevice* dev_get(const char* name, usize name_len) {
	for (usize i = 0; i < DEVICE_TYPE_MAX; ++i) {
		DeviceList* list = &DEVICES[i];
		mutex_lock(&list->lock);

		for (usize dev_i = 0; dev_i < list->len; ++dev_i) {
			GenericDevice* dev = list->devices[dev_i];
			if (strncmp(dev->name, name, name_len) == 0) {
				mutex_unlock(&list->lock);
				return dev;
			}
		}

		mutex_unlock(&list->lock);
	}

	return NULL;
}

void dev_remove(GenericDevice* device, CrescentDeviceType type) {
	DeviceList* list = &DEVICES[type];
	mutex_lock(&list->lock);

	for (usize i = 0; i < list->len; ++i) {
		if (list->devices[i] == device) {
			memmove(&list->devices[i], &list->devices[i + 1], (list->len - i - 1) * sizeof(GenericDevice*));
			break;
		}
	}

	mutex_unlock(&list->lock);
}

void sys_devmsg(void* state) {
	CrescentHandle handle = (CrescentHandle) *executor_arg0(state);
	size_t msg = (size_t) *executor_arg1(state);
	__user void* data = (__user void*) *executor_arg2(state);

	Task* task = arch_get_cur_task();
	HandleEntry* entry = handle_tab_get(&task->process->handle_table, handle);
	if (!entry) {
		*executor_ret0(state) = ERR_INVALID_ARG;
		return;
	}
	GenericDevice* device = (GenericDevice*) entry->data;

	switch (device->type) {
		case DEVICE_TYPE_FB:
			*executor_ret0(state) = fbdev_devmsg(container_of(device, FbDev, generic), msg, data);
			break;
		case DEVICE_TYPE_SND:
			assert(0 && "not implemented");
		case DEVICE_TYPE_PARTITION:
			assert(0 && "not implemented");
		case DEVICE_TYPE_MAX:
			*executor_ret0(state) = ERR_INVALID_ARG;
			break;
	}
}

void sys_devenum(void* state) {
	CrescentDeviceType type = (CrescentDeviceType) *executor_arg0(state);
	__user CrescentDeviceInfo* res = (__user CrescentDeviceInfo*) *executor_arg1(state);
	__user size_t* count = (__user size_t*) *executor_arg2(state);

	if (type >= DEVICE_TYPE_MAX) {
		*executor_ret0(state) = ERR_INVALID_ARG;
		return;
	}

	DeviceList* list = &DEVICES[type];

	mutex_lock(&list->lock);
	if (!res) {
		size_t kernel_count = list->len;

		mutex_unlock(&list->lock);
		if (!mem_copy_to_user(count, &kernel_count, sizeof(size_t))) {
			*executor_ret0(state) = ERR_FAULT;
			return;
		}
		*executor_ret0(state) = 0;
		return;
	}

	size_t len;
	if (!mem_copy_to_kernel(&len, count, sizeof(size_t))) {
		mutex_unlock(&list->lock);
		*executor_ret0(state) = ERR_FAULT;
		return;
	}
	len = MIN(len, list->len);

	Task* task = arch_get_cur_task();
	for (size_t i = 0; i < len; ++i) {
		GenericDevice* ptr = list->devices[i];
		// todo support removing devices
		ptr->refcount += 1;
		CrescentHandle handle = handle_tab_insert(&task->process->handle_table, ptr, HANDLE_TYPE_DEVICE);
		CrescentDeviceInfo info = {
			.handle = handle
		};
		strncpy(info.name, ptr->name, sizeof(info.name));
		if (!mem_copy_to_user(res + i, &info, sizeof(info))) {
			mutex_unlock(&list->lock);
			*executor_ret0(state) = ERR_FAULT;
			return;
		}
	}

	mutex_unlock(&DEVICES[type].lock);

	if (!mem_copy_to_user(count, &len, sizeof(size_t))) {
		*executor_ret0(state) = ERR_FAULT;
		return;
	}

	*executor_ret0(state) = 0;
}
