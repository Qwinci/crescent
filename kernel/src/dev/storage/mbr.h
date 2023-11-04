#pragma once
#include "types.h"

typedef struct {
	u8 status;
	u8 first_abs_sector_chs[3];
	u8 type;
	u8 last_abs_sector_chs[3];
	u32 first_abs_sector_lba;
	u32 num_of_sectors;
} MbrEntry;