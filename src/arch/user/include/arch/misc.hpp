#pragma once
#include <stdlib.h>

extern bool IRQS_ENABLED;

static inline bool arch_enable_irqs(bool enable) {
	auto old = IRQS_ENABLED;
	IRQS_ENABLED = enable;
	return old;
}

static inline void arch_hlt() {
	abort();
}
