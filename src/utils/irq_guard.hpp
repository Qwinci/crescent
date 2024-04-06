#pragma once
#include "arch/misc.hpp"

struct IrqGuard {
	IrqGuard() {
		old = arch_enable_irqs(false);
	}
	~IrqGuard() {
		arch_enable_irqs(old);
	}

private:
	bool old;
};
