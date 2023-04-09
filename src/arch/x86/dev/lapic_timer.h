#pragma once
#include "dev/timer.h"

typedef struct {
	usize freq;
	usize period_us;
	usize last_sched_us;
	usize us;
} LapicTimer;

void lapic_timer_init();
void lapic_timer_start(usize freq);
usize lapic_timer_get_ns();