#include "ext2.h"
#include "dev/storage/storage.h"
#include "mem/allocator.h"
#include "utils/math.h"
#include "stdio.h"
#include "fs/vfs.h"
#include "mem/utils.h"
#include "assert.h"
#include "sys/fs.h"
#include "crescent/sys.h"

typedef struct {
	u32 s_inodes_count;
	u32 s_blocks_count;
	u32 s_r_blocks_count;
	u32 s_free_blocks_count;
	u32 s_free_inodes_count;
	u32 s_first_data_block;
	u32 s_log_block_size;
	u32 s_log_frag_size;
	u32 s_blocks_per_group;
	u32 s_frags_per_group;
	u32 s_inodes_per_group;
	u32 s_mtime;
	u32 s_wtime;
	u16 s_mnt_count;
	u16 s_max_mnt_count;
	u16 s_magic;
	u16 s_state;
	u16 s_errors;
	u16 s_minor_rev_level;
	u32 s_lastcheck;
	u32 s_checkinterval;
	u32 s_creator_os;
	u32 s_rev_level;
	u16 s_def_resuid;
	u16 s_def_resgid;

	// ext2 dynamic rev
	u32 s_first_ino;
	u16 s_inode_size;
	u16 s_block_group_nr;
	u32 s_feature_compat;
	u32 s_feature_incompat;
	u32 s_feature_ro_compat;
	u8 s_uuid[16];
	char s_volume_name[16];
	u8 s_last_mounted[64];
	u32 s_algo_bitmap;

	// performance hints
	u8 s_prealloc_blocks;
	u8 s_prealloc_dir_blocks;
	u16 unused1;

	// journaling support
	u8 s_journal_uuid[16];
	u32 s_journal_inum;
	u32 s_journal_dev;
	u32 s_last_orphan;

	// directory indexing support
	u32 s_hash_seed[4];
	u8 s_def_hash_version;
	u8 unused2[3];

	// other options
	u32 s_default_mount_options;
	u32 s_first_meta_bg;
	u8 unused3[760];
} Ext2SuperBlock;

typedef struct {
	u32 bg_block_bitmap;
	u32 bg_inode_bitmap;
	u32 bg_inode_table;
	u16 bg_free_blocks_count;
	u16 bg_free_inodes_count;
	u16 bg_used_dirs_count;
	u8 unused[14];
} BlockGroupDesc;

typedef struct {
	u16 i_mode;
	u16 i_uid;
	u32 i_size;
	u32 i_atime;
	u32 i_ctime;
	u32 i_mtime;
	u32 i_dtime;
	u16 i_gid;
	u16 i_links_count;
	u32 i_blocks;
	u32 i_flags;
	u32 i_osd1;
	u32 i_block[15];
	u32 i_generation;
	u32 i_file_acl;
	u32 i_dir_acl;
	u32 i_faddr;
	u8 i_osd2[12];
} InodeDesc;

#define INODE_BLOCK_INDIRECT 12
#define INODE_BLOCK_DOUBLE_INDIRECT 13
#define INODE_BLOCK_TRIPLE_INDIRECT 14

typedef struct {
	u32 inode;
	u16 rec_len;
	u8 name_len;
	u8 file_type;
	char name[];
} LinkedDirEntryDesc;

typedef struct {
	Storage* owner;
	Vfs vfs;
	PartitionDev partition_dev;
	void* tmp_block_mem;
	usize start_blk;
	usize device_blks_in_ext_blk;

	BlockGroupDesc* block_groups;

	u32 block_size;

	u32 total_inodes;
	u32 total_blocks;
	u32 total_groups;

	u32 inodes_in_group;
	u32 blocks_in_group;

	u32 inode_size;
	u32 groups_size;
} Ext2Partition;

void* ext2_block_read(Ext2Partition* self, usize block) {
	if (!self->owner->read(self->owner, self->tmp_block_mem, self->start_blk + block * self->device_blks_in_ext_blk, self->device_blks_in_ext_blk)) {
		return NULL;
	}
	return offset(self->tmp_block_mem, void*, (block * self->block_size) % self->owner->blk_size);
}

InodeDesc ext2_inode_read(Ext2Partition* self, u32 num) {
	BlockGroupDesc* block_group = &self->block_groups[(num - 1) / self->inodes_in_group];
	u32 inode_idx = (num - 1) % self->inodes_in_group;

	u32 inode_byte_off = inode_idx * self->inode_size;
	u32 inode_block = block_group->bg_inode_table + (inode_byte_off / self->block_size);
	u32 inode_block_off = inode_byte_off % self->block_size;

	void* block = ext2_block_read(self, inode_block);
	assert(block);
	return *offset(block, InodeDesc*, inode_block_off);
}

typedef struct {
	u16 eh_magic;
	u16 eh_entries;
	u16 eh_max;
	u16 eh_depth;
	u32 eh_generation;
} Ext4ExtentHeader;

typedef struct {
	u32 ei_block;
	u32 ei_leaf_lo;
	u16 ei_leaf_hi;
	u16 ei_unused;
} Ext4ExtentIdx;

typedef struct {
	u32 ee_block;
	u16 ee_len;
	u16 ee_start_hi;
	u32 ee_start_lo;
} Ext4Extent;

static Ext4ExtentHeader* ext4_find_leaf(Ext2Partition* self, Ext4ExtentHeader* hdr, usize block) {
	while (true) {
		if (hdr->eh_depth == 0) {
			return hdr;
		}

		Ext4ExtentIdx* extents = offset(hdr, Ext4ExtentIdx*, sizeof(Ext4ExtentHeader));
		i32 i = -1;
		if (hdr->eh_entries == 1 && extents[0].ei_block == block) {
			i = 0;
		}
		else {
			for (; i < hdr->eh_entries; ++i) {
				Ext4ExtentIdx* extent = &extents[i];
				if (extent->ei_block > block) {
					i = i - 1;
					break;
				}
			}
		}

		if (i == -1) {
			return NULL;
		}

		u64 leaf_block = extents[i].ei_leaf_lo | (u64) extents[i].ei_leaf_hi << 32;
		hdr = (Ext4ExtentHeader*) ext2_block_read(self, leaf_block);
	}
}

static usize ext4_inode_data_read_helper(Ext2Partition* self, Ext4ExtentHeader* extent_hdr, usize off, void* data, usize max_read) {
	assert(extent_hdr->eh_magic == 0xF30A);

	usize read = 0;
	while (read < max_read) {
		usize block = (off + read) / self->block_size;
		usize offset = (off + read) % self->block_size;
		usize remaining = max_read - read;
		usize chunk = remaining > self->block_size - offset ? self->block_size - offset : remaining;

		Ext4ExtentHeader* leaf = ext4_find_leaf(self, extent_hdr, block);
		Ext4Extent* extents = offset(leaf, Ext4Extent*, sizeof(Ext4ExtentHeader));
		i32 i;
		if (leaf->eh_entries == 1 && extents[0].ee_block == block) {
			i = 0;
		}
		else {
			for (i = 0; i < leaf->eh_entries; ++i) {
				Ext4Extent* extent = &extents[i];
				if (extent->ee_block > block) {
					i = i - 1;
					break;
				}
			}
		}

		if (i == -1) {
			return read;
		}

		Ext4Extent* extent = &extents[i];

		if (block < extent->ee_block + extent->ee_len) {
			u64 extent_block = extent->ee_start_lo | (u64) extent->ee_start_hi << 32;
			extent_block += block - extent->ee_block;
			void* ptr = ext2_block_read(self, extent_block);
			assert(ptr);
			memcpy(offset(data, void*, read), offset(ptr, void*, offset), chunk);
		}
		else {
			memset(offset(data, void*, read), 0, chunk);
		}

		read += chunk;
	}
	return read;
}

int ext4_inode_data_read(Ext2Partition* self, InodeDesc* inode, usize off, void* data, usize size) {
	assert(inode->i_flags & 0x80000);
	if (off + size > inode->i_size) {
		return ERR_INVALID_ARG;
	}

	Ext4ExtentHeader* extent_hdr = (Ext4ExtentHeader*) inode->i_block;
	assert(ext4_inode_data_read_helper(self, extent_hdr, off, data, size) == size);
	return 0;
}

static usize ext2_vfs_read_dir_internal(VNode* self, usize off, LinkedDirEntryDesc** dir) {
	if (self->type != VNODE_DIR) {
		return 0;
	}

	InodeDesc* inode = (InodeDesc*) self->inode;
	if (off >= inode->i_size) {
		return 0;
	}

	LinkedDirEntryDesc* dirs = (LinkedDirEntryDesc*) self->data;
	LinkedDirEntryDesc* desc = offset(dirs, LinkedDirEntryDesc*, off);
	off += desc->rec_len;

	*dir = desc;
	return off;
}

static int ext2_vfs_read_dir(VNode* self, usize* offset, DirEntry* entry) {
	if (self->type != VNODE_DIR) {
		return ERR_NOT_DIR;
	}

	LinkedDirEntryDesc* desc;
	usize new_off = ext2_vfs_read_dir_internal(self, *offset, &desc);

	if (!new_off) {
		return ERR_NOT_EXISTS;
	}
	*offset = new_off;

	if (!desc->name_len) {
		return ERR_NOT_EXISTS;
	}
	memcpy(entry->name, desc->name, desc->name_len);
	entry->name_len = desc->name_len;
	entry->name[entry->name_len] = 0;
	entry->type = desc->file_type == 2 ? FS_ENTRY_TYPE_DIR : FS_ENTRY_TYPE_FILE;
	return 0;
}

static int ext2_vfs_lookup(VNode* self, Str component, VNode** ret);
static void ext2_vfs_release(VNode* self);
static int ext2_vfs_read(VNode* self, void* data, usize off, usize size);
static int ext2_vfs_stat(VNode* self, Stat* stat);

static VNode* ext2_populate_vnode(Vfs* vfs, VNodeType type, void* data, const InodeDesc* inode) {
	VNode* node = vnode_alloc();
	if (!node) {
		return NULL;
	}

	node->vfs = vfs;

	VNodeOps ops = {
		.read_dir = ext2_vfs_read_dir,
		.lookup = ext2_vfs_lookup,
		.read = ext2_vfs_read,
		.stat = ext2_vfs_stat,
		.release = ext2_vfs_release
	};

	node->ops = ops;
	node->data = data;
	node->inode = kmalloc(sizeof(InodeDesc));
	if (!node->inode) {
		vnode_free(node);
		return NULL;
	}
	*(InodeDesc*) node->inode = *inode;
	node->type = type;
	node->refcount = 1;
	return node;
}

static int ext2_vfs_lookup(VNode* self, Str component, VNode** ret) {
	if (self->type != VNODE_DIR) {
		return ERR_NOT_DIR;
	}

	usize off = 0;
	LinkedDirEntryDesc* entry;
	while ((off = ext2_vfs_read_dir_internal(self, off, &entry))) {
		Str entry_name = str_new_with_len(entry->name, entry->name_len);
		if (str_cmp(entry_name, component)) {
			Ext2Partition* partition = (Ext2Partition*) self->vfs->data;
			InodeDesc inode = ext2_inode_read(partition, entry->inode);

			if (entry->file_type == 2) {
				void* data = kmalloc(inode.i_size);
				if (!data) {
					return ERR_NO_MEM;
				}
				ext4_inode_data_read(partition, &inode, 0, data, inode.i_size);

				*ret = ext2_populate_vnode(self->vfs, VNODE_DIR, data, &inode);
				return *ret ? 0 : ERR_NO_MEM;
			}
			else {
				*ret = ext2_populate_vnode(self->vfs, VNODE_FILE, NULL, &inode);
				return *ret ? 0 : ERR_NO_MEM;
			}
		}
	}

	return ERR_NOT_EXISTS;
}

static int ext2_vfs_read(VNode* self, void* data, usize off, usize size) {
	if (self->type != VNODE_FILE) {
		return ERR_OPERATION_NOT_SUPPORTED;
	}

	InodeDesc* inode = (InodeDesc*) self->inode;

	Ext2Partition* partition = (Ext2Partition*) self->vfs->data;
	return ext4_inode_data_read(partition, inode, off, data, size);
}

static int ext2_vfs_stat(VNode* self, Stat* stat) {
	InodeDesc* inode = (InodeDesc*) self->inode;
	stat->size = inode->i_size;
	return 0;
}

static void ext2_vfs_release(VNode* self) {
	if (--self->refcount) {
		return;
	}

	if (self->type == VNODE_DIR) {
		kfree(self->data, ((InodeDesc*) self->inode)->i_size);
	}
	kfree(self->inode, sizeof(InodeDesc));
	vnode_free(self);
}

static VNode* ext2_vfs_get_root(Vfs* vfs) {
	Ext2Partition* self = (Ext2Partition*) vfs->data;

	InodeDesc root_inode = ext2_inode_read(self, 2);

	void* data = kmalloc(root_inode.i_size);
	if (!data) {
		return NULL;
	}
	ext4_inode_data_read(self, &root_inode, 0, data, root_inode.i_size);

	VNode* node = ext2_populate_vnode(vfs, VNODE_DIR, data, &root_inode);

	return node;
}

Vfs ext2_create_vfs(Ext2Partition* self) {
	return (Vfs) {
		.get_root = ext2_vfs_get_root,
		.data = self
	};
}

static int NUMBER = 0;

bool ext2_enum_partition(Storage* storage, usize start_blk, usize end_blk) {
	usize total_blks = end_blk - start_blk;
	usize super_block_start_blk = 1024 / storage->blk_size;
	usize super_block_blks = (sizeof(Ext2SuperBlock) + storage->blk_size - 1) / storage->blk_size;
	if (total_blks < super_block_blks) {
		return false;
	}

	Ext2SuperBlock* super_block = kcalloc(MAX(sizeof(Ext2SuperBlock), storage->blk_size));
	assert(super_block);
	if (!storage->read(storage, super_block, start_blk + super_block_start_blk, super_block_blks)) {
		kfree(super_block, MAX(sizeof(Ext2SuperBlock), storage->blk_size));
		return false;
	}

	if (super_block->s_magic != 0xEF53) {
		kfree(super_block, MAX(sizeof(Ext2SuperBlock), storage->blk_size));
		return false;
	}

	if (super_block->s_state != 1) {
		kfree(super_block, MAX(sizeof(Ext2SuperBlock), storage->blk_size));
		kprintf("[kernel][fs][ext2]: filesystem has errors, refusing to mount\n");
		return true;
	}

	Ext2Partition* self = kcalloc(sizeof(Ext2Partition));
	assert(self);

	self->owner = storage;
	self->start_blk = start_blk;
	self->block_size = 1024 << super_block->s_log_block_size;
	self->device_blks_in_ext_blk = (self->block_size + storage->blk_size - 1) / storage->blk_size;
	self->tmp_block_mem = kmalloc(self->device_blks_in_ext_blk * storage->blk_size);
	assert(self->tmp_block_mem);
	self->total_blocks = super_block->s_blocks_count;

	self->total_inodes = super_block->s_inodes_count;
	self->total_blocks = super_block->s_blocks_count;
	self->inodes_in_group = super_block->s_inodes_per_group;
	self->blocks_in_group = super_block->s_blocks_per_group;
	self->total_groups = (self->total_blocks + self->blocks_in_group - 1) / self->blocks_in_group;

	if (self->total_groups != (self->total_inodes + self->inodes_in_group - 1) / self->inodes_in_group) {
		kprintf("[kernel][fs][ext2]: error: block group count mismatch\n");
		kfree(super_block, MAX(sizeof(Ext2SuperBlock), storage->blk_size));
		kfree(self, sizeof(Ext2Partition));
		return true;
	}

	self->inode_size = super_block->s_rev_level == 1 ? super_block->s_inode_size : 128;
	self->groups_size = self->total_groups * sizeof(BlockGroupDesc);

	kfree(super_block, MAX(sizeof(Ext2SuperBlock), storage->blk_size));

	usize aligned_block_groups_size = ALIGNUP(self->groups_size, storage->blk_size);
	usize block_groups_blocks = aligned_block_groups_size / storage->blk_size;
	usize block_groups_starting_block = 2048 / storage->blk_size;
	usize block_groups_starting_off = 2048 % storage->blk_size;

	void* block_groups_mem = kmalloc(aligned_block_groups_size);
	assert(block_groups_mem);
	if (!storage->read(storage, block_groups_mem, start_blk + block_groups_starting_block, block_groups_blocks)) {
		kfree(self, sizeof(Ext2Partition));
		kfree(block_groups_mem, aligned_block_groups_size);
		return false;
	}

	self->block_groups = offset(block_groups_mem, BlockGroupDesc*, block_groups_starting_off);

	self->vfs = ext2_create_vfs(self);
	vfs_add(&self->vfs);
	self->partition_dev.vfs = &self->vfs;
	snprintf(self->partition_dev.generic.name, 128, "ext2_n%d", NUMBER++);
	self->partition_dev.generic.refcount = 0;
	dev_add(&self->partition_dev.generic, DEVICE_TYPE_PARTITION);

	return true;
}
