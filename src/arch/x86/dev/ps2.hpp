#pragma once
#include "types.hpp"
#include "array.hpp"

void x86_ps2_init();
bool ps2_has_data();
void ps2_send_data(bool second, u8 data);
void ps2_send_data2(bool second, u8 data1, u8 data2);
kstd::array<u8, 3> ps2_identify(bool second);
u8 ps2_receive_data();
