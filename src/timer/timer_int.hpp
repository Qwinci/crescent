#pragma once
#include "types.hpp"

struct InterruptFrame;

[[gnu::interrupt]] void timer_handler(InterruptFrame* frame);
extern u64 timer_ns_since_boot;