#include "handle.h"
#include "mem/allocator.h"
#include "string.h"
#include "sys/dev.h"
#include "fs/vfs.h"
#include "sys/fs.h"
#include "sched/task.h"

Handle handle_tab_insert(HandleTable* self, void* data, HandleType type) {
	assert(data);

	mutex_lock(&self->lock);

	if (self->freelist) {
		HandleEntry* entry = self->freelist;
		self->freelist = (HandleEntry*) entry->data;
		entry->data = data;
		entry->handle &= ~FREED_HANDLE;
		entry->type = type;
		entry->refs = 1;
		mutex_unlock(&self->lock);
		return entry->handle;
	}
	else {
		Handle id = self->size;
		if (id & FREED_HANDLE) {
			return INVALID_HANDLE;
		}

		self->cap = self->cap < 8 ? 8 : self->cap * 2;
		HandleEntry* table = kmalloc(sizeof(HandleEntry) * self->cap);
		if (!table) {
			mutex_unlock(&self->lock);
			return INVALID_HANDLE;
		}
		memset(table, 0, sizeof(HandleEntry) * self->cap);
		memcpy(table, self->table, sizeof(HandleEntry) * self->size);

		self->table = table;

		for (usize i = 1; i < self->cap - self->size; ++i) {
			HandleEntry* entry = &self->table[self->size + i];
			entry->handle = self->size + i;
			entry->handle |= FREED_HANDLE;
			entry->data = self->freelist;
			self->freelist = entry;
		}

		self->table[self->size++] = (HandleEntry) {
			.handle = id,
			.data = data,
			.type = type,
			.refs = 1
		};
		self->size = self->cap;
		mutex_unlock(&self->lock);
		return id;
	}
}

void handle_tab_destroy(HandleTable* self) {
	kfree(self->table, self->cap * sizeof(HandleEntry));
	*self = (HandleTable) {};
}

HandleEntry* handle_tab_get(HandleTable* self, Handle handle) {
	mutex_lock(&self->lock);
	if (handle >= self->size) {
		mutex_unlock(&self->lock);
		return NULL;
	}
	HandleEntry* entry = &self->table[handle];
	if (entry->handle & FREED_HANDLE) {
		mutex_unlock(&self->lock);
		return NULL;
	}
	mutex_unlock(&self->lock);
	return entry;
}

bool handle_tab_close(HandleTable* self, Handle handle) {
	mutex_lock(&self->lock);
	if (handle >= self->size) {
		mutex_unlock(&self->lock);
		return false;
	}
	HandleEntry* entry = &self->table[handle];
	if (entry->handle & FREED_HANDLE) {
		mutex_unlock(&self->lock);
		return false;
	}
	entry->refs -= 1;
	if (entry->refs) {
		mutex_unlock(&self->lock);
		return false;
	}
	entry->data = (void*) self->freelist;
	entry->handle |= FREED_HANDLE;
	self->freelist = entry;
	mutex_unlock(&self->lock);
	return true;
}

void* handle_tab_open(HandleTable* self, Handle handle) {
	mutex_lock(&self->lock);
	if (handle >= self->size || handle == INVALID_HANDLE) {
		mutex_unlock(&self->lock);
		return NULL;
	}
	HandleEntry* entry = &self->table[handle];
	if (entry->handle & FREED_HANDLE) {
		mutex_unlock(&self->lock);
		return NULL;
	}
	entry->refs += 1;
	void* data = entry->data;
	mutex_unlock(&self->lock);
	return data;
}

bool handle_tab_duplicate(HandleTable* self, HandleTable* ret) {
	memset(ret, 0, sizeof(HandleTable));
	ret->table = (HandleEntry*) kmalloc(self->cap * sizeof(HandleEntry));
	if (self->cap && !ret->table) {
		return false;
	}
	memcpy(ret->table, self->table, self->cap * sizeof(HandleEntry));
	ret->cap = self->cap;
	ret->size = self->size;
	for (HandleEntry* entry = self->freelist; entry; entry = (HandleEntry*) entry->data) {
		size_t index = ((usize) entry - (usize) self->table) / sizeof(HandleEntry);

		HandleEntry* ret_entry = &ret->table[index];
		ret_entry->handle = FREED_HANDLE;
		ret_entry->data = ret->freelist;
		ret->freelist = ret_entry;
	}

	for (size_t i = 0; i < ret->cap; ++i) {
		HandleEntry* entry = &ret->table[i];
		if (entry->handle & FREED_HANDLE) {
			continue;
		}
		HandleType type = entry->type;
		void* data = entry->data;

		switch (type) {
			case HANDLE_TYPE_THREAD:
			{
				ThreadHandle* h = (ThreadHandle*) data;
				if (--h->refcount == 0) {
					kfree(data, sizeof(ThreadHandle));
				}
				break;
			}
			case HANDLE_TYPE_DEVICE:
				((GenericDevice*) data)->refcount += 1;
				break;
			case HANDLE_TYPE_VNODE:
				((VNode*) data)->refcount += 1;
				break;
			case HANDLE_TYPE_KERNEL_GENERIC:
				continue;
			case HANDLE_TYPE_FILE:
			{
				FileData* copy = kmalloc(sizeof(FileData));
				if (!copy) {
					// todo free memory
					return false;
				}
				*copy = *(FileData*) data;
				copy->node->refcount += 1;
				break;
			}
			case HANDLE_TYPE_PROCESS:
				// todo refcount
				break;
		}
	}

	return true;
}
