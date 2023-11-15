#pragma once
#include "types.h"

#define COM1 0x3F8
#define COM2 0x2F8

bool serial_init(u16 port);
bool serial_can_receive(u16 port);
u8 serial_receive(u16 port);
void serial_send(u16 port, u8 value);
