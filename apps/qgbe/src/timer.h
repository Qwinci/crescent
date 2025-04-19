#pragma once
#include "types.h"

typedef struct {
	struct Bus* bus;
	u16 div;
	u8 tima;
	u8 tma;
	u8 tac;
	u8 last_res;
	bool overflowed;
	bool tima_reloaded;
} Timer;

void timer_write(Timer* self, u16 addr, u8 value);
u8 timer_read(Timer* self, u16 addr);
void timer_cycle(Timer* self);
