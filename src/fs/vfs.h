#pragma once
#include <stddef.h>

typedef enum {
	VNODE_FILE,
	VNODE_DIR
} VNodeType;

typedef struct VNode {
	struct Vfs* vfs;
	struct VNode* (*read_dir)(struct VNode* self);
	struct VNode* (*lookup)(struct VNode* self, const char* component);
	void (*release)(struct VNode* self);

	size_t cursor;
	size_t refcount;
	void* data;
	VNodeType type;
} VNode;

typedef struct Vfs {
	struct Vfs* next;
	VNode* (*get_root)(struct Vfs* self);
	void* data;
} Vfs;

extern Vfs* VFS_LIST;

VNode* vnode_alloc();
void vnode_free(VNode* vnode);
void vfs_add(Vfs* vfs);
