#include "dev.h"
#include "arch/cpu.h"
#include "crescent/sys.h"
#include "mem/allocator.h"
#include "string.h"
#include "mem/user.h"
#include "utils/math.h"
#include "dev/fb.h"

DeviceList DEVICES[DEVICE_TYPE_MAX] = {};

void dev_add(GenericDevice* device, DeviceType type) {
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

void dev_remove(GenericDevice* device, DeviceType type) {
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

int sys_devmsg(Handle handle, size_t msg, __user void* data) {
	Task* task = arch_get_cur_task();
	HandleEntry* entry = handle_tab_get(&task->process->handle_table, handle);
	if (!entry) {
		return ERR_INVALID_ARG;
	}
	GenericDevice* device = (GenericDevice*) entry->data;

	switch (device->type) {
		case DEVICE_TYPE_FB:
			return fbdev_devmsg(container_of(device, FbDev, generic), msg, data);
		case DEVICE_TYPE_MAX:
			return ERR_INVALID_ARG;
	}

	return 0;
}

int sys_devenum(DeviceType type, __user Handle* res, __user size_t* count) {
	if (type >= DEVICE_TYPE_MAX) {
		return ERR_INVALID_ARG;
	}

	DeviceList* list = &DEVICES[type];

	mutex_lock(&list->lock);
	if (!res) {
		size_t kernel_count = list->len;

		if (!mem_copy_to_user(count, &kernel_count, sizeof(size_t))) {
			mutex_unlock(&list->lock);
			return ERR_FAULT;
		}
		return 0;
	}

	size_t len;
	if (!mem_copy_to_kernel(&len, count, sizeof(size_t))) {
		mutex_unlock(&list->lock);
		return ERR_FAULT;
	}
	len = MIN(len, list->len);

	Task* task = arch_get_cur_task();
	for (size_t i = 0; i < len; ++i) {
		GenericDevice* ptr = list->devices[i];
		// todo support removing devices
		ptr->refcount += 1;
		Handle handle = handle_tab_insert(&task->process->handle_table, ptr, HANDLE_TYPE_GENERIC);
		if (!mem_copy_to_user(res + i, &handle, sizeof(Handle))) {
			mutex_unlock(&list->lock);
			return ERR_FAULT;
		}
	}

	if (!mem_copy_to_user(count, &len, sizeof(size_t))) {
		return ERR_FAULT;
	}

	mutex_unlock(&DEVICES[type].lock);
	return 0;
}
