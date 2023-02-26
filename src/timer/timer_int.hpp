#pragma once
#include "types.hpp"

struct InterruptCtx;

void timer_int(InterruptCtx*, void*);

void start_timer();
u64 get_timer_us();