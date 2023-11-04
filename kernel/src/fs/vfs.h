#pragma once
#include <stddef.h>
#include "utils/str.h"
#include "crescent/fs.h"

typedef enum {
	VNODE_FILE,
	VNODE_DIR
} VNodeType;

typedef struct VNode VNode;
typedef struct CrescentDir CrescentDir;

typedef struct {
	int (*read_dir)(VNode* self, usize* offset, CrescentDirEntry* entry);
	int (*lookup)(VNode* self, Str component, VNode** ret);
	int (*read)(VNode* self, void* data, usize off, usize size);
	int (*stat)(VNode* self, CrescentStat* stat);
	int (*write)(VNode* self, const void* data, usize off, usize size);
	void (*release)(VNode* self);
} VNodeOps;

typedef struct VNode {
	struct Vfs* vfs;
	VNodeOps ops;

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
