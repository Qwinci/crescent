#pragma once
#include "types.h"
#include "crescent/sys.h"

typedef struct Handle {
	struct Handle* prev;
	struct Handle* next;
	HandleId id;
} Handle;

#define HANDLE_LIST_HASH_TAB_COUNT 16

typedef struct {
	Handle* root;
	Handle* end;
} HandleHashTab;

typedef struct {
	HandleHashTab handles[HANDLE_LIST_HASH_TAB_COUNT];
	usize counter;
} HandleList;

void handle_list_add(HandleList* self, Handle* handle);
void handle_list_remove(HandleList* self, Handle* handle);
Handle* handle_list_get(HandleList* self, HandleId id);
