#pragma once
#include "types.h"
#include "crescent/sys.h"
#include "sched/mutex.h"

typedef struct {
	Handle handle;
	void* data;
} HandleEntry;

typedef struct {
	HandleEntry* table;
	usize size;
	usize cap;
	HandleEntry* freelist;
	Mutex lock;
} HandleTable;

#define INVALID_HANDLE ((Handle) -1)

Handle handle_tab_insert(HandleTable* self, void* data);
void handle_tab_remove(HandleTable* self, Handle handle);
void* handle_tab_get(HandleTable* self, Handle handle);
