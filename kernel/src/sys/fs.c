#include "fs.h"
#include "arch/cpu.h"
#include "crescent/fs.h"
#include "crescent/sys.h"
#include "fs/vfs.h"
#include "mem/allocator.h"
#include "mem/user.h"
#include "utils/math.h"

int kernel_fs_open_ex(const char* dev_name, usize dev_len, const char* path, VNode** ret) {
	const char* ptr = path;

	DeviceList* list = &DEVICES[DEVICE_TYPE_PARTITION];
	mutex_lock(&list->lock);

	GenericDevice* dev = NULL;
	for (usize dev_i = 0; dev_i < list->len; ++dev_i) {
		GenericDevice* d = list->devices[dev_i];
		if (strncmp(d->name, dev_name, dev_len) == 0) {
			dev = d;
			break;
		}
	}

	if (!dev) {
		mutex_unlock(&list->lock);
		return ERR_NOT_EXISTS;
	}

	PartitionDev* d = container_of(dev, PartitionDev, generic);
	VNode* root = d->vfs->get_root(d->vfs);
	if (!root) {
		mutex_unlock(&list->lock);
		return ERR_NO_MEM;
	}
	if (!*ptr) {
		mutex_unlock(&list->lock);
		*ret = root;
		return 0;
	}
	if (*ptr == '/') {
		++ptr;
	}

	VNode* node = root;
	while (true) {
		const char* start = ptr;
		for (; *ptr && *ptr != '/'; ++ptr);
		usize len = ptr - start;
		if (len == 0) {
			mutex_unlock(&list->lock);
			node->ops.release(node);

			return ERR_INVALID_ARG;
		}
		VNode* ret_node;
		int ret_status = node->ops.lookup(node, str_new_with_len(start, len), &ret_node);
		if (ret_status != 0) {
			mutex_unlock(&list->lock);
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

	*ret = node;
	return 0;
}

int kernel_fs_open(const char* path, VNode** ret) {
	const char* ptr = path;
	for (; *ptr && *ptr != '/'; ++ptr);
	usize dev_len = ptr - path;
	return kernel_fs_open_ex(path, dev_len, ptr, ret);
}

int sys_open(__user const char* path, size_t path_len, __user CrescentHandle* ret) {
	char* buf = kmalloc(path_len + 1);
	if (!buf) {
		return ERR_NO_MEM;
	}
	if (!mem_copy_to_kernel(buf, path, path_len)) {
		kfree(buf, path_len + 1);
		return ERR_FAULT;
	}
	buf[path_len] = 0;

	VNode* node;
	int status = kernel_fs_open(buf, &node);

	kfree(buf, path_len + 1);
	if (status != 0) {
		return status;
	}

	Task* task = arch_get_cur_task();
	FileData* data = kmalloc(sizeof(FileData));
	if (!data) {
		node->ops.release(node);
		return ERR_NO_MEM;
	}
	data->node = node;
	data->cursor = 0;

	CrescentHandle handle = handle_tab_insert(&task->process->handle_table, data, HANDLE_TYPE_FILE);
	if (!mem_copy_to_user(ret, &handle, sizeof(handle))) {
		handle_tab_close(&task->process->handle_table, handle);
		node->ops.release(node);
		kfree(data, sizeof(FileData));
		return ERR_FAULT;
	}
	return 0;
}

int sys_open_ex(__user const char* dev, size_t dev_len, __user const char* path, size_t path_len, __user CrescentHandle* ret) {
	char* dev_buf = kmalloc(dev_len);
	if (!dev_buf) {
		return ERR_NO_MEM;
	}
	if (!mem_copy_to_kernel(dev_buf, dev, dev_len)) {
		kfree(dev_buf, dev_len);
		return ERR_FAULT;
	}

	char* buf = kmalloc(path_len + 1);
	if (!buf) {
		kfree(dev_buf, dev_len);
		return ERR_NO_MEM;
	}
	if (!mem_copy_to_kernel(buf, path, path_len)) {
		kfree(dev_buf, dev_len);
		kfree(buf, path_len + 1);
		return ERR_FAULT;
	}
	buf[path_len] = 0;

	VNode* node;
	int status = kernel_fs_open_ex(dev_buf, dev_len, buf, &node);

	kfree(dev_buf, dev_len);
	kfree(buf, path_len + 1);
	if (status != 0) {
		return status;
	}

	Task* task = arch_get_cur_task();
	FileData* data = kmalloc(sizeof(FileData));
	if (!data) {
		node->ops.release(node);
		return ERR_NO_MEM;
	}
	data->node = node;
	data->cursor = 0;

	CrescentHandle handle = handle_tab_insert(&task->process->handle_table, data, HANDLE_TYPE_FILE);
	if (!mem_copy_to_user(ret, &handle, sizeof(handle))) {
		handle_tab_close(&task->process->handle_table, handle);
		node->ops.release(node);
		kfree(data, sizeof(FileData));
		return ERR_FAULT;
	}
	return 0;
}

int sys_read(CrescentHandle handle, __user void* buffer, size_t size) {
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

int sys_write(CrescentHandle handle, __user const void* buffer, size_t size) {
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
	if (!mem_copy_to_kernel(buf, buffer, size)) {
		kfree(buf, size);
		return ERR_FAULT;
	}

	int ret = data->node->ops.write(data->node, buf, data->cursor, size);
	if (ret == 0) {
		data->cursor += size;
	}
	kfree(buf, size);
	return ret;
}

int sys_seek(CrescentHandle handle, CrescentSeekType seek_type, CrescentSeekOff offset) {
	Task* task = arch_get_cur_task();
	HandleEntry* entry = handle_tab_get(&task->process->handle_table, handle);
	if (!entry || entry->type != HANDLE_TYPE_FILE) {
		return ERR_INVALID_ARG;
	}
	FileData* data = (FileData*) entry->data;

	switch (seek_type) {
		case KSEEK_SET:
			data->cursor = (usize) offset;
			break;
		case KSEEK_END:
		{
			CrescentStat s;
			int status;
			if ((status = data->node->ops.stat(data->node, &s)) != 0) {
				return status;
			}
			data->cursor = s.size + offset;
			break;
		}
		case KSEEK_CUR:
			data->cursor += offset;
			break;
	}
	return 0;
}

int sys_stat(CrescentHandle handle, __user CrescentStat* stat) {
	Task* task = arch_get_cur_task();
	HandleEntry* entry = handle_tab_get(&task->process->handle_table, handle);
	if (!entry || entry->type != HANDLE_TYPE_FILE) {
		return ERR_INVALID_ARG;
	}
	FileData* data = (FileData*) entry->data;

	CrescentStat s = {};
	int ret = data->node->ops.stat(data->node, &s);

	if (!mem_copy_to_user(stat, &s, sizeof(s))) {
		return ERR_FAULT;
	}
	return ret;
}

int sys_opendir(__user const char* path, size_t path_len, __user CrescentDir** ret) {
	int status = sys_open(path, path_len, (__user CrescentHandle*) ret);
	return status;
}

int sys_closedir(__user CrescentDir* dir) {
	Task* task = arch_get_cur_task();
	HandleEntry* entry = handle_tab_get(&task->process->handle_table, (CrescentHandle) dir);
	if (!entry || entry->type != HANDLE_TYPE_FILE) {
		return ERR_INVALID_ARG;
	}
	FileData* data = (FileData*) entry->data;

	data->node->ops.release(data->node);
	handle_tab_close(&task->process->handle_table, (CrescentHandle) dir);
	kfree(data, sizeof(FileData));
	return 0;
}

int sys_readdir(__user CrescentDir* dir, __user CrescentDirEntry* entry) {
	Task* task = arch_get_cur_task();
	HandleEntry* handle = handle_tab_get(&task->process->handle_table, (CrescentHandle) dir);
	if (!handle || handle->type != HANDLE_TYPE_FILE) {
		return ERR_INVALID_ARG;
	}
	FileData* data = (FileData*) handle->data;

	CrescentDirEntry kernel_entry;
	int ret = data->node->ops.read_dir(data->node, &data->cursor, &kernel_entry);
	if (ret != 0) {
		return ret;
	}
	if (!mem_copy_to_user(entry, &kernel_entry, sizeof(kernel_entry))) {
		return ERR_FAULT;
	}
	return 0;
}
