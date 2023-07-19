#include "tar.h"
#include "arch/mod.h"
#include "stdio.h"
#include "vfs.h"
#include "mem/allocator.h"
#include "mem/utils.h"

typedef struct {
	char filename[100];
	char mode[8];
	char uid[8];
	char gid[8];
	char size[12];
	char mtime[12];
	char chksum[8];
	char typeflag;
} TarHeader;

static u32 oct_to_int(const char* str) {
	u32 value = 0;
	while (*str >= '0' && *str <= '7') {
		value = value * 8 + (*str - '0');
		str += 1;
	}
	return value;
}

static VNode* tar_lookup(struct VNode* self, const char* component) {
	return NULL;
}

static VNode* tar_read_dir(struct VNode* self) {
	const TarHeader* hdr = offset(self->data, const TarHeader*, self->cursor);
	if (!hdr->filename[0] || self->type != VNODE_DIR) {
		return NULL;
	}

	VNode* node = vnode_alloc();
	if (!node) {
		return NULL;
	}

	VNodeType type = VNODE_FILE;
	if (hdr->typeflag == 0 || hdr->typeflag == '0') {
		type = VNODE_FILE;
	}
	else if (hdr->typeflag == '5') {
		type = VNODE_DIR;
	}

	*node = (VNode) {
		.vfs = self->vfs,
		.read_dir = tar_read_dir,
		.lookup = tar_lookup,
		.cursor = 0,
		.refcount = 1,
		.data = self->data,
		.type = type
	};

	self->cursor += 512 + ALIGNUP(oct_to_int(hdr->size), 512);

	return node;
}

static VNode* tar_get_root(Vfs* self) {
	VNode* node = vnode_alloc();
	if (!node) {
		return NULL;
	}

	*node = (VNode) {
		.vfs = self,
		.read_dir = tar_read_dir,
		.lookup = tar_lookup,
		.data = self->data,
		.refcount = 1,
		.type = VNODE_DIR
	};

	return node;
}

void tar_initramfs_init() {
	Module initramfs_mod = arch_get_module("initramfs.tar");
	if (!initramfs_mod.base) {
		kprintf("[kernel][fs][tar]: initramfs.tar was not found\n");
		return;
	}

	Vfs* tar_vfs = kmalloc(sizeof(Vfs));
	if (!tar_vfs) {
		kprintf("[kernel][fs][tar]: failed to allocate memory for vfs\n");
		return;
	}

	tar_vfs->get_root = tar_get_root;

	tar_vfs->data = initramfs_mod.base;

	vfs_add(tar_vfs);

	VNode* root = tar_vfs->get_root(tar_vfs);
	VNode* node;
	while ((node = root->read_dir(root))) {
		kprintf("a\n");
	}
}
