#include <stddef.h>
#include "vfs.h"
#include "sched/mutex.h"
#include "mem/allocator.h"

Vfs* VFS_LIST = NULL;
static Vfs* VFS_LIST_END = NULL;
static Mutex VFS_LIST_LOCK = {};

void vfs_add(Vfs* vfs) {
	mutex_lock(&VFS_LIST_LOCK);

	vfs->next = NULL;

	if (VFS_LIST) {
		vfs->prev = VFS_LIST_END;
		VFS_LIST_END->next = vfs;
		VFS_LIST_END = vfs;
	}
	else {
		vfs->prev = NULL;
		VFS_LIST = vfs;
		VFS_LIST_END = vfs;
	}

	mutex_unlock(&VFS_LIST_LOCK);
}

static VNode* VNODE_LIST = NULL;
static Mutex VNODE_LIST_MUTEX = {};

VNode* vnode_alloc() {
	mutex_lock(&VNODE_LIST_MUTEX);

	VNode* node;
	if (!VNODE_LIST) {
		node = kmalloc(sizeof(VNode));
	}
	else {
		node = VNODE_LIST;
		VNODE_LIST = (VNode*) node->data;
	}

	mutex_unlock(&VNODE_LIST_MUTEX);

	if (!node) {
		return NULL;
	}
	memset(node, 0, sizeof(VNode));
	return node;
}

void vnode_free(VNode* vnode) {
	mutex_lock(&VNODE_LIST_MUTEX);

	vnode->data = VNODE_LIST;
	VNODE_LIST = vnode;

	mutex_unlock(&VNODE_LIST_MUTEX);
}
