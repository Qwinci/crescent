#include "fat.h"
#include "assert.h"
#include "dev/storage/storage.h"
#include "mem/allocator.h"
#include "mem/utils.h"
#include "string.h"
#include "types.h"
#include "utils/math.h"

typedef struct {
	u8 bytes_per_sector[2];
	u8 sectors_per_cluster;
	u8 num_reserved_sectors[2];
	u8 num_fats;
	u8 num_root_entries[2];
	u8 num_sectors[2];
	u8 media;
	u8 sectors_per_fat[2];
	u8 sectors_per_track[2];
	u8 num_heads[2];
	u8 num_hidden_sectors[4];
	u8 num_large_sectors[4];
} PackedFatBpb;

typedef struct {
	u8 bytes_per_sector[2];
	u8 sectors_per_cluster;
	u8 reserved_sectors[2];
	u8 num_fats;
	u8 num_root_entries[2];
	u8 num_sectors[2];
	u8 media;
	u8 sectors_per_fat[2];
	u8 sectors_per_track[2];
	u8 num_heads[2];
	u8 num_hidden_sectors[4];
	u8 num_large_sectors[4];
	u8 large_sectors_per_fat[4];
	u8 extended_flags[2];
	u8 fs_version[2];
	u8 root_dir_first_cluster[4];
	u8 fs_info_sector[2];
	u8 backup_boot_sector[2];
	u8 reserved[12];
} PackedFatBpbEx;

typedef struct {
	u8 jump[3];
	u8 oem_ident[8];
	PackedFatBpb bpb;
	u8 phys_drive_num;
	u8 current_head;
	u8 signature;
	u8 id[4];
	u8 vol_label[11];
	u8 system_id[8];
} PackedFatBootSector;

typedef struct {
	u16 bytes_per_sector;
	u8 sectors_per_cluster;
	u16 num_reserved_sectors;
	u8 num_fats;
	u16 num_root_entries;
	u16 num_sectors;
	u8 media;
	u16 sectors_per_fat;
	u16 sectors_per_track;
	u16 num_heads;
	u32 num_hidden_sectors;
	u32 num_large_sectors;
	u32 large_sectors_per_fat;
	u16 extended_flags;
	u16 fs_version;
	u32 root_dir_first_cluster;
	u16 fs_info_sector;
	u16 backup_boot_sector;
} FatBpb;

static inline void fat_unpack_bpb(const PackedFatBpb* packed, FatBpb* res) {
	res->bytes_per_sector = packed->bytes_per_sector[0] | packed->bytes_per_sector[1] << 8;
	res->sectors_per_cluster = packed->sectors_per_cluster;
	res->num_reserved_sectors = packed->num_reserved_sectors[0] | packed->num_reserved_sectors[1] << 8;
	res->num_fats = packed->num_fats;
	res->num_root_entries = packed->num_root_entries[0] | packed->num_root_entries[1] << 8;
	res->num_sectors = packed->num_sectors[0] | packed->num_sectors[1] << 8;
	res->media = packed->media;
	res->sectors_per_fat = packed->sectors_per_fat[0] | packed->sectors_per_fat[1] << 8;
	res->sectors_per_track = packed->sectors_per_track[0] | packed->sectors_per_track[1] << 8;
	res->num_heads = packed->num_heads[0] | packed->num_heads[1] << 8;
	res->num_hidden_sectors =
		packed->num_hidden_sectors[0] |
		packed->num_hidden_sectors[1] << 8 |
		packed->num_hidden_sectors[2] << 16 |
		packed->num_hidden_sectors[3] << 24;
	res->num_large_sectors =
		packed->num_large_sectors[0] |
		packed->num_large_sectors[1] << 8 |
		packed->num_large_sectors[2] << 16 |
		packed->num_large_sectors[3] << 24;
	res->large_sectors_per_fat =
		((const PackedFatBpbEx*) packed)->large_sectors_per_fat[0] |
		((const PackedFatBpbEx*) packed)->large_sectors_per_fat[1] << 8 |
		((const PackedFatBpbEx*) packed)->large_sectors_per_fat[2] << 16 |
		((const PackedFatBpbEx*) packed)->large_sectors_per_fat[3] << 24;
	res->extended_flags =
		((const PackedFatBpbEx*) packed)->extended_flags[0] |
		((const PackedFatBpbEx*) packed)->extended_flags[1] << 8;
	res->fs_version =
		((const PackedFatBpbEx*) packed)->fs_version[0] |
		((const PackedFatBpbEx*) packed)->fs_version[1] << 8;
	res->root_dir_first_cluster =
		((const PackedFatBpbEx*) packed)->root_dir_first_cluster[0] |
		((const PackedFatBpbEx*) packed)->root_dir_first_cluster[1] << 8 |
		((const PackedFatBpbEx*) packed)->root_dir_first_cluster[2] << 16 |
		((const PackedFatBpbEx*) packed)->root_dir_first_cluster[3] << 24;
	res->fs_info_sector =
		((const PackedFatBpbEx*) packed)->fs_info_sector[0] |
		((const PackedFatBpbEx*) packed)->fs_info_sector[1] << 8;
	res->backup_boot_sector =
		((const PackedFatBpbEx*) packed)->backup_boot_sector[0] |
		((const PackedFatBpbEx*) packed)->backup_boot_sector[1] << 8;
}

#define FLAG_ACTIVE_FAT(flags) ((flags) & 0xF)
#define FLAG_MIRROR_DISABLED(flags) ((flags) & 1 << 7)

static bool fat_verify_bpb(const PackedFatBootSector* boot_sector, FatBpb* bpb) {
	bool res = true;

	fat_unpack_bpb(&boot_sector->bpb, bpb);

	if (bpb->num_sectors) {
		bpb->num_large_sectors = 0;
	}
	if (boot_sector->jump[0] != 0xE9 && boot_sector->jump[0] != 0xEB && boot_sector->jump[0] != 0x49) {
		res = false;
	}
	else if (bpb->bytes_per_sector != 128 &&
			 bpb->bytes_per_sector != 256 &&
			 bpb->bytes_per_sector != 512 &&
			 bpb->bytes_per_sector != 1024 &&
			 bpb->bytes_per_sector != 2048 &&
			 bpb->bytes_per_sector != 4096) {
		res = false;
	}
	else if (bpb->sectors_per_cluster != 1 &&
			 bpb->sectors_per_cluster != 2 &&
			 bpb->sectors_per_cluster != 4 &&
			 bpb->sectors_per_cluster != 8 &&
			 bpb->sectors_per_cluster != 16 &&
			 bpb->sectors_per_cluster != 32 &&
			 bpb->sectors_per_cluster != 64 &&
			 bpb->sectors_per_cluster != 128) {
		res = false;
	}
	else if (bpb->num_reserved_sectors == 0) {
		res = false;
	}
	else if (bpb->num_fats == 0) {
		res = false;
	}
	else if (bpb->num_sectors == 0 && bpb->num_large_sectors == 0) {
		res = false;
	}
	else if (bpb->sectors_per_fat == 0 && (bpb->large_sectors_per_fat == 0 || bpb->fs_version != 0)) {
		res = false;
	}
	else if (bpb->sectors_per_fat != 0 && bpb->num_root_entries == 0) {
		res = false;
	}
	else if (bpb->sectors_per_fat == 0 && FLAG_MIRROR_DISABLED(bpb->extended_flags)) {
		res = false;
	}

	return res;
}

typedef struct {
	u8 name[11];
	u8 attr;
	u8 reserved;
	u8 creation_time_tenth;
	union {
		struct {
			u16 creation_time;
			u16 creation_date;
			u16 last_access_date;
			u16 first_cluster_high;
			u16 write_time;
			u16 write_date;
		};
		u8 name2[12];
	};
	u16 first_cluster_low;
	union {
		u32 file_size;
		u8 name3[4];
	};
} FatDirectory;

typedef struct {
	usize start_blk;
	u32 first_data_sector;
	u8 sectors_per_cluster;
} FatPartition;

static inline usize cluster_to_sector(const FatPartition* self, u32 cluster) {
	return (cluster - 2) * self->sectors_per_cluster + self->first_data_sector;
}

bool fat_enum_partition(Storage* storage, usize start_blk, usize end_blk) {
	PackedFatBootSector* packed_boot_sector = (PackedFatBootSector*) kmalloc(storage->blk_size);
	assert(packed_boot_sector);
	assert(storage->read(storage, packed_boot_sector, start_blk, 1));

	FatBpb bpb = {};
	bool is_fat = fat_verify_bpb(packed_boot_sector, &bpb);
	kprintf("is_fat: %s\n", is_fat ? "true" : "false");
	if (!is_fat) {
		return false;
	}

	usize total_sectors = bpb.num_sectors ? bpb.num_sectors : bpb.num_large_sectors;
	usize fat_size_in_sectors = bpb.sectors_per_fat ? bpb.sectors_per_fat : bpb.large_sectors_per_fat;
	usize root_dir_sectors = ALIGNUP(bpb.num_root_entries * 32, bpb.bytes_per_sector) / bpb.bytes_per_sector;
	usize first_data_sector = bpb.num_reserved_sectors + (bpb.num_fats * fat_size_in_sectors) + root_dir_sectors;
	usize num_data_sectors = total_sectors - first_data_sector;
	usize num_clusters = num_data_sectors / bpb.sectors_per_cluster;

	u32 first_root_dir_sector;
	// fat12 or fat16
	if (num_clusters < 65525) {
		first_root_dir_sector = first_data_sector - root_dir_sectors;
	}
	// fat32
	else {
		u32 root_cluster = bpb.root_dir_first_cluster;
		usize first_sector = ((root_cluster - 2) * bpb.sectors_per_cluster) + first_data_sector;
		first_root_dir_sector = first_sector;
	}

	u32 blks_in_sector = ALIGNUP(bpb.bytes_per_sector, storage->blk_size) / storage->blk_size;

	u8* data = kmalloc(blks_in_sector * storage->blk_size);
	assert(data);
	assert(storage->read(storage, data, start_blk + blks_in_sector * first_root_dir_sector, blks_in_sector));

	const FatDirectory* dir = (const FatDirectory*) data;
	char* long_name = NULL;
	usize long_name_cap = 0;
	usize long_name_len = 0;

	usize current_offset = start_blk + blks_in_sector * first_root_dir_sector;
	isize data_remaining = (isize) (blks_in_sector * storage->blk_size);
	while (true) {
		if (data_remaining == 0) {
			current_offset += blks_in_sector * storage->blk_size;
			dir = (const FatDirectory*) data;
			assert(storage->read(storage, data, current_offset, blks_in_sector));
		}

		if (dir->name[0] == 0) {
			break;
		}
		// Unused entry
		else if (dir->name[0] == 0xE5) {
			++dir;
			data_remaining -= sizeof(*dir);
			continue;
		}
		// long file name
		if (dir->attr == 0xF) {
			if (long_name_len + 13 >= long_name_cap) {
				usize old_size = long_name_cap;
				long_name_cap = long_name_cap < 13 ? 13 : (long_name_cap * 2);
				char* new_name = (char*) kmalloc(long_name_cap + 1);
				assert(new_name);
				memcpy(new_name, long_name, long_name_len);
				new_name[long_name_len] = 0;
				kfree(long_name, old_size);
				long_name = new_name;
			}

			for (u8 i = 1; i < 11; i += 2) {
				long_name[long_name_len++] = (char) dir->name[i];
			}

			for (u8 i = 0; i < 12; i += 2) {
				long_name[long_name_len++] = (char) dir->name2[i];
			}

			for (u8 i = 0; i < 4; i += 2) {
				long_name[long_name_len++] = (char) dir->name3[i];
			}

			long_name[long_name_len] = 0;
		}
		else {
			if (long_name) {
				//kprintf("%s\n", long_name);
				kfree(long_name, long_name_cap + 1);
				long_name = NULL;
				long_name_len = 0;
				long_name_cap = 0;
			}
		}

		data_remaining -= sizeof(*dir);
		++dir;
	}

	kfree(data, blks_in_sector * storage->blk_size);

	return true;
}