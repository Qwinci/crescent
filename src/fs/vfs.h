#pragma once
#include <stddef.h>
#include "utils/str.h"
#include "crescent/fs.h"

typedef enum {
	VNODE_FILE,
	VNODE_DIR
} VNodeType;

typedef struct {
	usize size;
} Stat;

typedef struct VNode {
	struct Vfs* vfs;
	void* (*begin_read_dir)(struct VNode* self);
	void (*end_read_dir)(struct VNode* self, void* state);
	bool (*read_dir)(struct VNode* self, void* state, DirEntry* entry);
	struct VNode* (*lookup)(struct VNode* self, Str component);
	bool (*read)(struct VNode* self, void* data, usize off, usize size);
	bool (*stat)(struct VNode* self, Stat* stat);
	bool (*write)(struct VNode* self, const void* data, usize off, usize size);
	void (*release)(struct VNode* self);

	size_t cursor;
	size_t refcount;
	void* data;
	void* inode;
	VNodeType type;
} VNode;

typedef struct Vfs {
	struct Vfs* prev;
	struct Vfs* next;
	VNode* (*get_root)(struct Vfs* self);
	void* data;
} Vfs;

extern Vfs* VFS_LIST;

VNode* vnode_alloc();
void vnode_free(VNode* vnode);
void vfs_add(Vfs* vfs);
