#pragma once

struct InterruptFrame;

[[gnu::interrupt]] void timer_int(InterruptFrame*);