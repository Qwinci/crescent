#include "gpt.h"
#include "assert.h"
#include "dev/storage/fs/fat.h"
#include "mbr.h"
#include "mem/allocator.h"
#include "mem/utils.h"
#include "storage.h"
#include "string.h"
#include "types.h"
#include "utils/math.h"

typedef struct {
	char signature[8];
	u32 revision;
	u32 header_size;
	u32 crc32;
	u32 reserved;
	u64 current_lba;
	u64 backup_lba;
	u64 first_usable_lba;
	u64 last_usable_lba;
	u8 guid[16];
	u64 partition_list_start_lba;
	u32 num_of_partitions;
	u32 partition_entry_size;
	u32 crc32_partition_list;
} GptHeader;

typedef struct {
	u8 type_guid[16];
	u8 guid[16];
	u64 start_lba;
	u64 end_lba;
	u64 attributes;
	u16 name[];
} GptEntry;

bool gpt_verify_mbr(const MbrEntry mbr[4]) {
	MbrEntry empty = {};
	if (memcmp(&mbr[1], &empty, sizeof(MbrEntry)) != 0 ||
		memcmp(&mbr[2], &empty, sizeof(MbrEntry)) != 0 ||
		memcmp(&mbr[3], &empty, sizeof(MbrEntry)) != 0) {
		return false;
	}
	if (mbr[0].status != 0 ||
		mbr[0].first_abs_sector_chs[0] != 0 ||
		mbr[0].first_abs_sector_chs[1] != 2 ||
		mbr[0].first_abs_sector_chs[2] != 0) {
		return false;
	}
	if (mbr[0].type != 0xEE ||
		mbr[0].first_abs_sector_lba != 1) {
		return false;
	}
	return true;
}

bool gpt_enum_partitions(Storage* storage) {
	if (storage->blk_count < 2) {
		return false;
	}

	void* mbr = kmalloc(storage->blk_size);
	assert(mbr);
	assert(storage->read(storage, mbr, 0, 1));
	const MbrEntry* mbr_entries = offset(mbr, const MbrEntry*, 0x1BE);

	GptHeader* gpt = (GptHeader*) kmalloc(storage->blk_size);
	assert(gpt);
	assert(storage->read(storage, gpt, 1, 1));

	if (strncmp(gpt->signature, "EFI PART", 8) != 0) {
		return false;
	}

	if (!gpt_verify_mbr(mbr_entries)) {
		return false;
	}

	kfree(mbr, storage->blk_size);

	usize gpt_list_blocks = ALIGNUP(gpt->num_of_partitions * gpt->partition_entry_size, storage->blk_size) / storage->blk_size;
	GptEntry* partition_entries = kmalloc(ALIGNUP(gpt->num_of_partitions * gpt->partition_entry_size, storage->blk_size));
	assert(partition_entries);
	usize transferred_blks = 0;
	while (transferred_blks < gpt_list_blocks) {
		usize read = MIN(gpt_list_blocks - transferred_blks, storage->max_transfer_blk);
		assert(storage->read(
			storage,
			offset(partition_entries, void*, transferred_blks * storage->blk_size),
			gpt->partition_list_start_lba + transferred_blks,
			read));
		transferred_blks += read;
	}

	for (usize i = 0; i < gpt->num_of_partitions; ++i) {
		const GptEntry* entry = offset(partition_entries, const GptEntry*, i * gpt->partition_entry_size);

		u8 empty_guid[16] = {};
		if (memcmp(entry->guid, empty_guid, 16) == 0) {
			continue;
		}

		usize name_len = gpt->partition_entry_size - sizeof(GptEntry);
		kprintf("[kernel][fs][gpt]: gpt entry '");
		for (usize j = 0; j < name_len; ++j) {
			u16 codepoint = entry->name[j];
			if (codepoint == 0) {
				break;
			}
			// todo properly convert and print
			kprintf("%c", (char) codepoint);
		}
		kprintf("'\n");

		if (entry->start_lba >= storage->blk_count || entry->start_lba >= entry->end_lba) {
			kprintf("[kernel][fs][gpt]: gpt entry is invalid\n");
			continue;
		}

		fat_enum_partition(storage, entry->start_lba, entry->end_lba);
	}

	kfree(partition_entries, ALIGNUP(gpt->num_of_partitions * gpt->partition_entry_size, storage->blk_size));

	return true;
}