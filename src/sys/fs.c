#include "fs.h"
#include "crescent/fs.h"
#include "fs/vfs.h"
#include "mem/user.h"
#include "crescent/sys.h"
#include "mem/allocator.h"
#include "arch/cpu.h"

int sys_open(__user const char* path, size_t path_len, __user Handle* ret) {
	char* buf = kmalloc(path_len + 1);
	if (!buf) {
		return ERR_NO_MEM;
	}
	if (!mem_copy_to_kernel(buf, path, path_len)) {
		kfree(buf, path_len + 1);
		return ERR_FAULT;
	}
	buf[path_len] = 0;

	char* ptr = buf;
	for (; *ptr && *ptr != '/'; ++ptr);
	usize dev_len = ptr - buf;

	DeviceList* list = &DEVICES[DEVICE_TYPE_PARTITION];
	mutex_lock(&list->lock);

	GenericDevice* dev = NULL;
	for (usize dev_i = 0; dev_i < list->len; ++dev_i) {
		GenericDevice* d = list->devices[dev_i];
		if (strncmp(d->name, buf, dev_len) == 0) {
			dev = d;
			break;
		}
	}

	if (!dev) {
		mutex_unlock(&list->lock);
		kfree(buf, path_len + 1);
		return ERR_NOT_EXISTS;
	}

	PartitionDev* d = container_of(dev, PartitionDev, generic);
	VNode* root = d->vfs->get_root(d->vfs);
	if (!root) {
		mutex_unlock(&list->lock);
		kfree(buf, path_len + 1);
		return ERR_NO_MEM;
	}
	if (!*ptr) {
		mutex_unlock(&list->lock);
		kfree(buf, path_len + 1);

		Task* task = arch_get_cur_task();

		FileData* data = kmalloc(sizeof(FileData));
		if (!data) {
			root->ops.release(root);
			return ERR_NO_MEM;
		}
		data->node = root;
		data->cursor = 0;
		Handle handle = handle_tab_insert(&task->process->handle_table, data, HANDLE_TYPE_FILE);
		if (!mem_copy_to_user(ret, &handle, sizeof(handle))) {
			root->ops.release(root);
			handle_tab_close(&task->process->handle_table, handle);
			kfree(data, sizeof(FileData));
			return ERR_FAULT;
		}
		return 0;
	}
	++ptr;

	VNode* node = root;
	while (true) {
		const char* start = ptr;
		for (; *ptr && *ptr != '/'; ++ptr);
		usize len = ptr - start;
		if (len == 0) {
			mutex_unlock(&list->lock);
			kfree(buf, path_len + 1);
			node->ops.release(node);

			return ERR_INVALID_ARG;
		}
		VNode* ret_node;
		int ret_status = node->ops.lookup(node, str_new_with_len(start, len), &ret_node);
		if (ret_status != 0) {
			mutex_unlock(&list->lock);
			kfree(buf, path_len + 1);

			node->ops.release(node);
			return ret_status;
		}
		node->ops.release(node);
		node = ret_node;
		if (!*ptr) {
			break;
		}
		++ptr;
	}

	mutex_unlock(&list->lock);
	kfree(buf, path_len + 1);

	Task* task = arch_get_cur_task();
	FileData* data = kmalloc(sizeof(FileData));
	if (!data) {
		node->ops.release(node);
		return ERR_NO_MEM;
	}
	data->node = node;
	data->cursor = 0;

	Handle handle = handle_tab_insert(&task->process->handle_table, data, HANDLE_TYPE_FILE);
	if (!mem_copy_to_user(ret, &handle, sizeof(handle))) {
		handle_tab_close(&task->process->handle_table, handle);
		node->ops.release(node);
		kfree(data, sizeof(FileData));
		return ERR_FAULT;
	}
	return 0;
}

int sys_read(Handle handle, __user void* buffer, size_t size) {
	Task* task = arch_get_cur_task();
	HandleEntry* entry = handle_tab_get(&task->process->handle_table, handle);
	if (!entry || entry->type != HANDLE_TYPE_FILE) {
		return ERR_INVALID_ARG;
	}
	FileData* data = (FileData*) entry->data;

	char* buf = kmalloc(size);
	if (!buf) {
		return ERR_NO_MEM;
	}

	int ret = data->node->ops.read(data->node, buf, data->cursor, size);
	if (ret == 0) {
		data->cursor += size;
	}
	if (!mem_copy_to_user(buffer, buf, size)) {
		kfree(buf, size);
		return ERR_FAULT;
	}
	kfree(buf, size);
	return ret;
}

int sys_stat(Handle handle, __user Stat* stat) {
	Task* task = arch_get_cur_task();
	HandleEntry* entry = handle_tab_get(&task->process->handle_table, handle);
	if (!entry || entry->type != HANDLE_TYPE_FILE) {
		return ERR_INVALID_ARG;
	}
	FileData* data = (FileData*) entry->data;

	Stat s = {};
	int ret = data->node->ops.stat(data->node, &s);

	if (!mem_copy_to_user(stat, &s, sizeof(s))) {
		return ERR_FAULT;
	}
	return ret;
}

int sys_opendir(__user const char* path, size_t path_len, __user Dir** ret) {
	int status = sys_open(path, path_len, (__user Handle*) ret);
	return status;
}

int sys_closedir(__user Dir* dir) {
	Task* task = arch_get_cur_task();
	HandleEntry* entry = handle_tab_get(&task->process->handle_table, (Handle) dir);
	if (!entry || entry->type != HANDLE_TYPE_FILE) {
		return ERR_INVALID_ARG;
	}
	FileData* data = (FileData*) entry->data;

	data->node->ops.release(data->node);
	handle_tab_close(&task->process->handle_table, (Handle) dir);
	kfree(data, sizeof(FileData));
	return 0;
}

int sys_readdir(__user Dir* dir, __user DirEntry* entry) {
	Task* task = arch_get_cur_task();
	HandleEntry* handle = handle_tab_get(&task->process->handle_table, (Handle) dir);
	if (!handle || handle->type != HANDLE_TYPE_FILE) {
		return ERR_INVALID_ARG;
	}
	FileData* data = (FileData*) handle->data;

	DirEntry kernel_entry;
	int ret = data->node->ops.read_dir(data->node, &data->cursor, &kernel_entry);
	if (ret != 0) {
		return ret;
	}
	if (!mem_copy_to_user(entry, &kernel_entry, sizeof(kernel_entry))) {
		return ERR_FAULT;
	}
	return 0;
}
