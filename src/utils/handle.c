#include "handle.h"
#include "utils/math.h"

void handle_list_add(HandleList* self, Handle* handle) {
	usize hash = murmur64(handle->id);
	HandleHashTab* tab = &self->handles[hash % HANDLE_LIST_HASH_TAB_COUNT];
	handle->next = NULL;
	if (!tab->root) {
		handle->prev = NULL;
		tab->root = handle;
		tab->end = handle;
	}
	else {
		tab->end->next = handle;
		handle->prev = tab->end;
		tab->end = handle;
	}
}

void handle_list_remove(HandleList* self, Handle* handle) {
	usize hash = murmur64(handle->id);
	HandleHashTab* tab = &self->handles[hash % HANDLE_LIST_HASH_TAB_COUNT];
	if (handle->prev) {
		handle->prev->next = handle->next;
	}
	else {
		tab->root = handle->next;
	}
	if (handle->next) {
		handle->next->prev = handle->prev;
	}
	else {
		tab->end = handle->prev;
	}
}

Handle* handle_list_get(HandleList* self, HandleId id) {
	usize hash = murmur64(id);
	HandleHashTab* tab = &self->handles[hash % HANDLE_LIST_HASH_TAB_COUNT];
	for (Handle* handle = tab->root; handle; handle = handle->next) {
		if (handle->id == id) {
			return handle;
		}
	}
	return NULL;
}