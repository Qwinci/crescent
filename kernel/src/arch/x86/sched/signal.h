#pragma once

typedef struct InterruptCtx InterruptCtx;

void process_possible_signals(InterruptCtx* ctx);
