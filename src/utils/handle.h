#pragma once
#include "types.h"
#include "crescent/handle.h"
#include "sched/mutex.h"

typedef enum {
	HANDLE_TYPE_THREAD,
	HANDLE_TYPE_DEVICE,
	HANDLE_TYPE_VNODE,
	HANDLE_TYPE_KERNEL_GENERIC
} HandleType;

typedef struct {
	Handle handle;
	void* data;
	HandleType type;
	unsigned int refs;
} HandleEntry;

typedef struct {
	HandleEntry* table;
	usize size;
	usize cap;
	HandleEntry* freelist;
	Mutex lock;
} HandleTable;

#define FREED_HANDLE (1ULL << (sizeof(usize) * 8 - 1))

Handle handle_tab_insert(HandleTable* self, void* data, HandleType type);
HandleEntry* handle_tab_get(HandleTable* self, Handle handle);
bool handle_tab_close(HandleTable* self, Handle handle);
void* handle_tab_open(HandleTable* self, Handle handle);
void handle_tab_destroy(HandleTable* self);
