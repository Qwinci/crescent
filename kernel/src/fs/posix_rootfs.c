#include "posix_rootfs.h"
#include "assert.h"
#include "crescent/sys.h"
#include "mem/allocator.h"
#include "sys/fs.h"
#include "vfs.h"

typedef struct Node {
	char name[128];
	Vfs* proxy_root;
	u16 name_len;
} Node;

typedef struct {
	Vfs vfs;
	PartitionDev partition_dev;
	Vfs* root_proxy;
	Node* dirs;
	usize dirs_len;
	usize dirs_cap;
} PosixRootfs;

static Node create_proxy_dir(const char* name, u16 name_len, Vfs* proxy_root) {
	Node node = {
		.proxy_root = proxy_root,
		.name_len = name_len
	};
	memcpy(node.name, name, name_len);
	node.name[name_len] = 0;
	return node;
}

static VNodeOps POSIX_ROOTFS_OPS;

static VNode* posix_rootfs_populate_root(Vfs* vfs) {
	VNode* node = vnode_alloc();
	if (!node) {
		return NULL;
	}

	node->vfs = vfs;

	node->ops = POSIX_ROOTFS_OPS;
	node->type = VNODE_DIR;
	node->refcount = 1;
	return node;
}

static int posix_rootfs_vfs_read_dir(VNode* self, usize* offset, CrescentDirEntry* entry) {
	PosixRootfs* fs = container_of(self->vfs, PosixRootfs, vfs);
	const usize MASK = 1ULL << ((sizeof(uintptr_t) * 8) - 1);
	if (!(*offset & MASK)) {
		VNode* proxy_root = fs->root_proxy->get_root(fs->root_proxy);
		if (!proxy_root) {
			return ERR_NO_MEM;
		}
		if (proxy_root->ops.read_dir(proxy_root, offset, entry) == 0) {
			proxy_root->ops.release(proxy_root);
			return 0;
		}
		proxy_root->ops.release(proxy_root);
		*offset = MASK;
	}

	usize index = *offset & ~MASK;

	if (index >= fs->dirs_len) {
		return ERR_NOT_EXISTS;
	}
	*offset += 1;
	Node* child = &fs->dirs[index];
	entry->type = FS_ENTRY_TYPE_DIR;
	entry->name_len = child->name_len;
	memcpy(entry->name, child->name, child->name_len);
	entry->name[child->name_len] = 0;

	return 0;
}

static int posix_rootfs_vfs_lookup(VNode* self, Str component, VNode** ret) {
	PosixRootfs* fs = container_of(self->vfs, PosixRootfs, vfs);

	VNode* proxy_root = fs->root_proxy->get_root(fs->root_proxy);
	if (!proxy_root) {
		return ERR_NO_MEM;
	}
	if (proxy_root->ops.lookup(proxy_root, component, ret) == 0) {
		proxy_root->ops.release(proxy_root);
		return 0;
	}
	proxy_root->ops.release(proxy_root);

	for (usize i = 0; i < fs->dirs_len; ++i) {
		Node* child = &fs->dirs[i];
		if (str_cmp(str_new_with_len(child->name, child->name_len), component)) {
			*ret = child->proxy_root->get_root(child->proxy_root);
			if (!*ret) {
				return ERR_NO_MEM;
			}
			return 0;
		}
	}

	return ERR_NOT_EXISTS;
}

static int posix_rootfs_vfs_read(VNode* self, void* data, usize off, usize size) {
	return ERR_OPERATION_NOT_SUPPORTED;
}

static int posix_rootfs_vfs_stat(VNode* self, CrescentStat* stat) {
	return ERR_OPERATION_NOT_SUPPORTED;
}

static int posix_rootfs_vfs_write(VNode* self, const void* data, usize off, usize size) {
	return ERR_OPERATION_NOT_SUPPORTED;
}

static void posix_rootfs_vfs_release(VNode* self) {
	vnode_free(self);
}

static VNodeOps POSIX_ROOTFS_OPS = {
	.read_dir = posix_rootfs_vfs_read_dir,
	.lookup = posix_rootfs_vfs_lookup,
	.read = posix_rootfs_vfs_read,
	.stat = posix_rootfs_vfs_stat,
	.write = posix_rootfs_vfs_write,
	.release = posix_rootfs_vfs_release
};

static VNode* posix_rootfs_vfs_get_root(Vfs* vfs) {
	return posix_rootfs_populate_root(vfs);
}

static int posix_rootfs_add_dir(PosixRootfs* self, const Node* dir) {
	if (self->dirs_len == self->dirs_cap) {
		usize new_cap = self->dirs_cap < 8 ? 8 : self->dirs_cap * 2;
		Node* new_dirs = (Node*) krealloc(self->dirs, self->dirs_cap * sizeof(Node), new_cap * sizeof(Node));
		if (!new_dirs) {
			return ERR_NO_MEM;
		}
		self->dirs_cap = new_cap;
		self->dirs = new_dirs;
	}
	self->dirs[self->dirs_len++] = *dir;
	return 0;
}

void posix_rootfs_init(Vfs* root_proxy) {
	PosixRootfs* self = kcalloc(sizeof(PosixRootfs));
	assert(self);

	self->root_proxy = root_proxy;
	self->vfs.get_root = posix_rootfs_vfs_get_root;
	self->vfs.data = self;

	vfs_add(&self->vfs);
	self->partition_dev.vfs = &self->vfs;
	memcpy(self->partition_dev.generic.name, "posix_rootfs", sizeof("posix_rootfs"));
	self->partition_dev.generic.refcount = 0;
	dev_add(&self->partition_dev.generic, DEVICE_TYPE_PARTITION);
}
