#include "tar.h"
#include "arch/mod.h"
#include "stdio.h"
#include "vfs.h"
#include "mem/allocator.h"
#include "mem/utils.h"
#include "assert.h"
#include "sys/fs.h"
#include "crescent/sys.h"

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

static u64 oct_to_int(const char* str) {
	u64 value = 0;
	while (*str >= '0' && *str <= '7') {
		value = value * 8 + (*str - '0');
		str += 1;
	}
	return value;
}

static int tar_vfs_read_dir(VNode* self, usize* offset, DirEntry* entry);
static int tar_vfs_lookup(VNode* self, Str component, VNode** ret);
static int tar_vfs_read(VNode* self, void* data, usize off, usize size);
static int tar_vfs_stat(VNode* self, Stat* stat);
static void tar_vfs_release(VNode* self);

static VNode* tar_populate_vnode(Vfs* vfs, void* data, VNodeType type) {
	VNode* node = vnode_alloc();
	if (!node) {
		return NULL;
	}
	node->vfs = vfs;

	VNodeOps ops = {
		.read_dir = tar_vfs_read_dir,
		.lookup = tar_vfs_lookup,
		.read = tar_vfs_read,
		.stat = tar_vfs_stat,
		.release = tar_vfs_release
	};

	node->ops = ops;
	node->data = data;
	node->type = type;
	node->refcount = 1;

	return node;
}

static int tar_vfs_read_dir(VNode* self, usize* offset, DirEntry* entry) {
	if (self->type != VNODE_DIR) {
		return ERR_NOT_DIR;
	}

	const TarHeader* cur_hdr = (const TarHeader*) self->data;
	const char* dir_end = cur_hdr->filename;
	for (const char* s = cur_hdr->filename; *s; ++s) {
		if (*s == '/') {
			dir_end = s;
		}
	}
	usize dir_len = dir_end - cur_hdr->filename;

	const TarHeader* next_hdr = offset(cur_hdr, const TarHeader*, *offset + 512 + ALIGNUP(oct_to_int(cur_hdr->size), 512));
	if (!next_hdr->filename[0]) {
		return false;
	}

	while (1) {
		const char* next_dir_end = next_hdr->filename;
		for (const char* s = next_hdr->filename; *s; ++s) {
			if (*s == '/' && s[1]) {
				next_dir_end = s;
			}
		}
		usize next_dir_len = next_dir_end - next_hdr->filename;
		if (next_dir_len == dir_len && str_cmp(
			str_new_with_len(cur_hdr->filename, dir_len),
			str_new_with_len(next_hdr->filename, next_dir_len))) {
			break;
		}
		else {
			*offset += 512 + ALIGNUP(oct_to_int(next_hdr->size), 512);
			next_hdr = offset(next_hdr, const TarHeader*, 512 + ALIGNUP(oct_to_int(next_hdr->size), 512));

			if (!next_hdr->filename[0]) {
				return ERR_NOT_EXISTS;
			}
		}
	}

	*offset += 512 + ALIGNUP(oct_to_int(next_hdr->size), 512);

	usize len = strlen(next_hdr->filename);
	entry->name_len = len - (dir_len + 1) - (next_hdr->filename[len - 1] == '/' ? 1 : 0);
	memcpy(entry->name, next_hdr->filename + dir_len + 1, entry->name_len);
	entry->name[entry->name_len] = 0;
	entry->type = next_hdr->typeflag == '5' ? FS_ENTRY_TYPE_DIR : FS_ENTRY_TYPE_FILE;

	return 0;
}

static int tar_vfs_lookup(VNode* self, Str component, VNode** ret) {
	if (self->type != VNODE_DIR) {
		return ERR_NOT_DIR;
	}

	const TarHeader* cur_hdr = (const TarHeader*) self->data;
	const char* dir_end = cur_hdr->filename;
	for (const char* s = cur_hdr->filename; *s; ++s) {
		if (*s == '/') {
			dir_end = s;
		}
	}
	usize dir_len = dir_end - cur_hdr->filename;

	const TarHeader* next_hdr = offset(cur_hdr, const TarHeader*, 512 + ALIGNUP(oct_to_int(cur_hdr->size), 512));

	while (1) {
		const char* next_dir_end = next_hdr->filename;
		usize next_len = 0;
		for (const char* s = next_hdr->filename; *s; ++s) {
			if (*s == '/' && s[1]) {
				next_dir_end = s;
			}
			++next_len;
		}
		usize next_dir_len = next_dir_end - next_hdr->filename;
		Str next_str = str_new_with_len(
			next_dir_end + 1,
			next_len - (next_dir_len + 1) - (next_hdr->filename[next_len - 1] == '/' ? 1 : 0));
		if (next_dir_len == dir_len && str_cmp(next_str, component)) {
			break;
		}
		else {
			next_hdr = offset(next_hdr, const TarHeader*, 512 + ALIGNUP(oct_to_int(next_hdr->size), 512));
			if (!next_hdr->filename[0]) {
				return ERR_NOT_EXISTS;
			}
		}
	}

	VNodeType type = next_hdr->typeflag == '5' ? VNODE_DIR : VNODE_FILE;
	*ret = tar_populate_vnode(self->vfs, (void*) next_hdr, type);
	return 0;
}

static int tar_vfs_read(VNode* self, void* data, usize off, usize size) {
	const TarHeader* hdr = (const TarHeader*) self->data;
	if (hdr->typeflag == '5') {
		return ERR_OPERATION_NOT_SUPPORTED;
	}

	usize file_size = oct_to_int(hdr->size);
	if (off + size > file_size) {
		return ERR_INVALID_ARG;
	}
	memcpy(data, offset(hdr, void*, 512 + off), size);

	return 0;
}

static int tar_vfs_stat(VNode* self, Stat* stat) {
	const TarHeader* hdr = (const TarHeader*) self->data;
	stat->size = oct_to_int(hdr->size);
	return 0;
}

static void tar_vfs_release(VNode* self) {
	if (--self->refcount) {
		return;
	}
	vnode_free(self);
}

static VNode* tar_get_root(Vfs* self) {
	return tar_populate_vnode(self, self->data, VNODE_DIR);
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

	PartitionDev* dev = kcalloc(sizeof(PartitionDev));
	assert(dev);

	memcpy(dev->generic.name, "initramfs", sizeof("initramfs"));

	dev_add(&dev->generic, DEVICE_TYPE_PARTITION);
}
