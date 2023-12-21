#pragma once
#include "types.h"
#include "crescent/handle.h"
#include "sched/mutex.h"

typedef enum {
	HANDLE_TYPE_THREAD,
	HANDLE_TYPE_PROCESS,
	HANDLE_TYPE_DEVICE,
	HANDLE_TYPE_VNODE,
	HANDLE_TYPE_FILE,
	HANDLE_TYPE_KERNEL_GENERIC
} HandleType;

typedef struct {
	CrescentHandle handle;
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

CrescentHandle handle_tab_insert(HandleTable* self, void* data, HandleType type);
HandleEntry* handle_tab_get(HandleTable* self, CrescentHandle handle);
bool handle_tab_close(HandleTable* self, CrescentHandle handle);
void* handle_tab_open(HandleTable* self, CrescentHandle handle);
void handle_tab_destroy(HandleTable* self);
bool handle_tab_duplicate(HandleTable* self, HandleTable* ret);
