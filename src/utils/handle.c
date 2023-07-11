#include "handle.h"
#include "mem/allocator.h"
#include "string.h"

#define FREED_HANDLE (1ULL << (sizeof(usize) * 8 - 1))

Handle handle_tab_insert(HandleTable* self, void* data, HandleType type) {
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
		self->cap = self->cap < 8 ? 8 : self->cap * 2;
		HandleEntry* table = kmalloc(sizeof(HandleEntry) * self->cap);
		if (!table) {
			mutex_unlock(&self->lock);
			return INVALID_HANDLE;
		}
		memset(table, 0, sizeof(HandleEntry) * self->cap);
		memcpy(table, self->table, sizeof(HandleEntry) * self->size);
		self->table = table;
		Handle id = self->size;
		if (id & FREED_HANDLE) {
			return INVALID_HANDLE;
		}
		self->table[self->size++] = (HandleEntry) {
			.handle = id,
			.data = data,
			.type = type,
			.refs = 1
		};
		mutex_unlock(&self->lock);
		return id;
	}
}

void handle_tab_close(HandleTable* self, Handle handle) {
	mutex_lock(&self->lock);
	if (handle >= self->size) {
		mutex_unlock(&self->lock);
		return;
	}
	HandleEntry* entry = &self->table[handle];
	if (entry->handle & FREED_HANDLE) {
		mutex_unlock(&self->lock);
		return;
	}
	if (entry->refs) {
		entry->refs -= 1;
		mutex_unlock(&self->lock);
		return;
	}
	entry->data = (void*) self->freelist;
	entry->handle |= FREED_HANDLE;
	self->freelist = entry;
	mutex_unlock(&self->lock);
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
