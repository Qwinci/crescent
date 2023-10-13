#include "fs.h"
#include "crescent/fs.h"
#include "fs/vfs.h"
#include "mem/user.h"
#include "crescent/sys.h"
#include "mem/allocator.h"
#include "arch/cpu.h"

typedef struct {
	Handle handle;
	__user const char* component;
	usize component_len;
} KernelFsOpenData;

typedef struct {
	Handle handle;
	Handle state;
	DirEntry entry;
} KernelFsReadDirData;

typedef struct {
	Handle handle;
	__user void* buffer;
	usize len;
} KernelFsReadData;

typedef struct {
	Handle handle;
	__user const void* buffer;
	usize len;
} KernelFsWriteData;

int partition_dev_devmsg(PartitionDev* self, DevMsgFs msg, __user void* data) {
	switch (msg) {
		case DEVMSG_FS_OPEN:
		{
			Task* task = arch_get_cur_task();
			KernelFsOpenData open_data;
			if (!mem_copy_to_kernel(&open_data, data, sizeof(KernelFsOpenData))) {
				return ERR_FAULT;
			}

			if (open_data.handle == INVALID_HANDLE) {
				VNode* node = self->vfs->get_root(self->vfs);
				if (!node) {
					return ERR_NO_MEM;
				}
				Handle handle = handle_tab_insert(&task->process->handle_table, node, HANDLE_TYPE_VNODE);
				if (handle == INVALID_HANDLE) {
					node->release(node);
				}
				open_data.handle = handle;
			}
			else {
				HandleEntry* node_entry = handle_tab_get(&task->process->handle_table, open_data.handle);
				if (!node_entry) {
					return ERR_INVALID_ARG;
				}
				VNode* node = (VNode*) node_entry->data;

				char* buffer = kmalloc(open_data.component_len);
				if (!buffer) {
					return ERR_NO_MEM;
				}
				if (!mem_copy_to_kernel(buffer, open_data.component, open_data.component_len)) {
					kfree(buffer, open_data.component_len);
					return ERR_FAULT;
				}

				Str str = str_new_with_len(buffer, open_data.component_len);
				VNode* new_node = node->lookup(node, str);
				kfree(buffer, open_data.component_len);
				// todo separate no mem and no file
				if (!new_node) {
					return ERR_INVALID_ARG;
				}

				Handle handle = handle_tab_insert(&task->process->handle_table, new_node, HANDLE_TYPE_VNODE);
				if (handle == INVALID_HANDLE) {
					new_node->release(new_node);
				}
				open_data.handle = handle;
			}

			if (!mem_copy_to_user(data, &open_data, sizeof(KernelFsOpenData))) {
				HandleEntry* entry = handle_tab_get(&task->process->handle_table, open_data.handle);
				if (entry) {
					VNode* node = (VNode*) entry->data;
					node->release(node);
					handle_tab_close(&task->process->handle_table, open_data.handle);
				}
				return ERR_FAULT;
			}

			return 0;
		}
		case DEVMSG_FS_READDIR:
		{
			Task* task = arch_get_cur_task();
			KernelFsReadDirData readdir_data;
			if (!mem_copy_to_kernel(&readdir_data, data, sizeof(KernelFsReadDirData))) {
				return ERR_FAULT;
			}

			HandleEntry* entry = handle_tab_get(&task->process->handle_table, readdir_data.handle);
			if (!entry) {
				return ERR_INVALID_ARG;
			}
			VNode* node = (VNode*) entry->data;

			HandleEntry* state_entry = readdir_data.state ? handle_tab_get(&task->process->handle_table, readdir_data.state) : NULL;
			void* state;
			if (state_entry) {
				state = state_entry->data;
			}
			else {
				state = node->begin_read_dir(node);
				readdir_data.state = handle_tab_insert(&task->process->handle_table, state, HANDLE_TYPE_KERNEL_GENERIC);
			}

			if (!node->read_dir(node, state, &readdir_data.entry)) {
				handle_tab_close(&task->process->handle_table, readdir_data.state);
				node->end_read_dir(node, state);
				return ERR_NOT_EXISTS;
			}

			if (!mem_copy_to_user(data, &readdir_data, sizeof(KernelFsReadDirData))) {
				handle_tab_close(&task->process->handle_table, readdir_data.state);
				node->end_read_dir(node, state);
				return ERR_FAULT;
			}
			return 0;
		}
		case DEVMSG_FS_READ:
		{
			Task* task = arch_get_cur_task();
			KernelFsReadData read_data;
			if (!mem_copy_to_kernel(&read_data, data, sizeof(KernelFsReadData))) {
				return ERR_FAULT;
			}

			HandleEntry* entry = handle_tab_get(&task->process->handle_table, read_data.handle);
			if (!entry) {
				return ERR_INVALID_ARG;
			}
			VNode* node = (VNode*) entry->data;

			void* buffer = kmalloc(read_data.len);
			if (!buffer) {
				return ERR_NO_MEM;
			}
			if (!node->read(node, buffer, node->cursor, read_data.len)) {
				read_data.len = 0;
				if (!mem_copy_to_user(data, &read_data, sizeof(KernelFsReadData))) {
					kfree(buffer, read_data.len);
					return ERR_FAULT;
				}
			}
			else {
				node->cursor += read_data.len;
			}

			if (!mem_copy_to_user(read_data.buffer, buffer, read_data.len)) {
				kfree(buffer, read_data.len);
				return ERR_FAULT;
			}
			kfree(buffer, read_data.len);

			return 0;
		}
		case DEVMSG_FS_WRITE:
		{
			Task* task = arch_get_cur_task();
			KernelFsWriteData write_data;
			if (!mem_copy_to_kernel(&write_data, data, sizeof(KernelFsWriteData))) {
				return ERR_FAULT;
			}

			HandleEntry* entry = handle_tab_get(&task->process->handle_table, write_data.handle);
			if (!entry) {
				return ERR_INVALID_ARG;
			}
			VNode* node = (VNode*) entry->data;

			void* buffer = kmalloc(write_data.len);
			if (!buffer) {
				return ERR_NO_MEM;
			}
			if (!mem_copy_to_kernel(buffer, write_data.buffer, write_data.len)) {
				kfree(buffer, write_data.len);
				return ERR_FAULT;
			}

			if (!node->write(node, buffer, node->cursor, write_data.len)) {
				write_data.len = 0;
				if (!mem_copy_to_user(data, &write_data, sizeof(KernelFsWriteData))) {
					kfree(buffer, write_data.len);
					return ERR_FAULT;
				}
			}
			else {
				node->cursor += write_data.len;
			}

			kfree(buffer, write_data.len);

			return 0;
		}
		case DEVMSG_FS_STAT:
		{
			Task* task = arch_get_cur_task();
			FsStatData stat_data;
			if (!mem_copy_to_kernel(&stat_data, data, sizeof(FsStatData))) {
				return ERR_FAULT;
			}

			HandleEntry* entry = handle_tab_get(&task->process->handle_table, stat_data.handle);
			if (!entry) {
				return ERR_INVALID_ARG;
			}
			VNode* node = (VNode*) entry->data;

			Stat stat;
			if (!node->stat(node, &stat)) {
				// todo better error
				return ERR_INVALID_ARG;
			}

			stat_data.size = stat.size;
			if (!mem_copy_to_user(data, &stat_data, sizeof(FsStatData))) {
				return ERR_FAULT;
			}

			return 0;
		}
	}
}