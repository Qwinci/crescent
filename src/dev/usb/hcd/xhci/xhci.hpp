#pragma once
#include "mem/iospace.hpp"
#include "functional.hpp"
#include "utils/driver.hpp"

struct IrqHandler;

namespace xhci {
	struct GenericOps {
		void (*enable_irqs)(void* arg, bool enable);
		bool (*install_irq)(void* arg, IrqHandler* handler);
		void (*uninstall_irq)(void* arg, IrqHandler* handler);
		void* arg;
	};

	InitStatus init(const GenericOps& ops, IoSpace space);
}
