#pragma once
#include "types.h"

typedef struct Storage {
	bool (*write)(struct Storage* self, void* buf, usize start, usize count);
	bool (*read)(struct Storage* self, void* buf, usize start, usize count);
	usize blk_count;
	usize blk_size;
	usize max_transfer_blk;
	struct Storage* next;
} Storage;

extern Storage* STORAGES;

void storage_register(Storage* storage);
void storage_enum_partitions(Storage* self);