#pragma once
#include "types.h"

void ps2_init();
u8 ps2_status_read();
u8 ps2_data_read();
void ps2_data_write(u8 data);
bool ps2_data2_write(u8 data);
bool ps2_wait_for_output();

#define PS2_STATUS_OUTPUT_FULL (1 << 0)
#define PS2_DISABLE_SCANNING 0xF5
#define PS2_ENABLE_SCANNING 0xF4