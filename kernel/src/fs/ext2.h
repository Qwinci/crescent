#pragma once
#include "types.h"

typedef struct Storage Storage;

bool ext2_enum_partition(Storage* storage, usize start_blk, usize end_blk);
