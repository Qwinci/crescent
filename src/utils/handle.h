#pragma once
#include "types.h"
#include "crescent/sys.h"
#include "sched/mutex.h"

typedef enum {
	HANDLE_TYPE_THREAD
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

#define INVALID_HANDLE ((Handle) -1)

Handle handle_tab_insert(HandleTable* self, void* data, HandleType type);
void handle_tab_close(HandleTable* self, Handle handle);
void* handle_tab_open(HandleTable* self, Handle handle);
