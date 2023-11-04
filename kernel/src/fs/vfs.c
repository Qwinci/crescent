#include <stddef.h>
#include "vfs.h"
#include "sched/mutex.h"
#include "mem/allocator.h"
#include "crescent/sys.h"

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

static int dummy_read_dir(VNode* self, usize* offset, DirEntry* entry) {
	return ERR_OPERATION_NOT_SUPPORTED;
}
static int dummy_lookup(VNode* self, Str component, VNode** ret) {
	return ERR_OPERATION_NOT_SUPPORTED;
}
static int dummy_read(VNode* self, void* data, usize off, usize size) {
	return ERR_OPERATION_NOT_SUPPORTED;
}
static int dummy_stat(VNode* self, Stat* stat) {
	return ERR_OPERATION_NOT_SUPPORTED;
}
static int dummy_write(VNode* self, const void* data, usize off, usize size) {
	return ERR_OPERATION_NOT_SUPPORTED;
}
static void dummy_release(VNode* self) {}

static const VNodeOps DUMMY_OPS = {
	.read_dir = dummy_read_dir,
	.lookup = dummy_lookup,
	.read = dummy_read,
	.stat = dummy_stat,
	.write = dummy_write,
	.release = dummy_release
};

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
	node->ops = DUMMY_OPS;

	return node;
}

void vnode_free(VNode* vnode) {
	mutex_lock(&VNODE_LIST_MUTEX);

	vnode->data = VNODE_LIST;
	VNODE_LIST = vnode;

	mutex_unlock(&VNODE_LIST_MUTEX);
}
