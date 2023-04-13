#pragma once
#include "types.h"

void ps2_init();
u8 ps2_status_read();
u8 ps2_data_read();

#define PS2_STATUS_OUTPUT_FULL (1 << 0)