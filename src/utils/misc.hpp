#pragma once

static inline void swapgs() {
	asm volatile("swapgs" : : : "memory");
}