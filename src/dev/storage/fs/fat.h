#pragma once
#include "types.h"

typedef struct Storage Storage;

bool fat_enum_partition(Storage* storage, usize start_blk, usize end_blk);