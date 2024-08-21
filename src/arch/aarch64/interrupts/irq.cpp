#include "exceptions.hpp"
#include "arch/aarch64/dev/gic.hpp"
#include "arch/irq.hpp"
#include "arch/cpu.hpp"
#include "sched/sched.hpp"
#include "vector.hpp"
#include "stdio.hpp"

namespace {
	ManuallyDestroy<kstd::vector<DoubleList<IrqHandler, &IrqHandler::hook>>> IRQ_HANDLERS;
	Spinlock<void> IRQ_HANDLERS_LOCK {};
}

void aarch64_irq_init() {
	IRQ_HANDLERS->resize(GIC->num_irqs);
}

void register_irq_handler(u32 num, IrqHandler* handler) {
	IrqGuard irq_guard {};
	auto guard = IRQ_HANDLERS_LOCK.lock();
	if (!(*IRQ_HANDLERS)[num].is_empty()) {
		if (!(*IRQ_HANDLERS)[num].front()->can_be_shared || !handler->can_be_shared) {
			return;
		}
	}
	(*IRQ_HANDLERS)[num].push(handler);
}

void deregister_irq_handler(u32 num, IrqHandler* handler) {
	IrqGuard irq_guard {};
	auto guard = IRQ_HANDLERS_LOCK.lock();
	if ((*IRQ_HANDLERS)[num].is_empty()) {
		return;
	}
	(*IRQ_HANDLERS)[num].remove(handler);
}

extern "C" [[gnu::used]] void arch_irq_handler(ExceptionFrame* frame) {
	IrqGuard irq_guard {};

	auto num = GIC->ack();
	if (num >= GIC->num_irqs) {
		GIC->eoi(num);
		return;
	}

	bool handler_found = false;
	{
		auto guard = IRQ_HANDLERS_LOCK.lock();
		for (auto& handler : (*IRQ_HANDLERS)[num]) {
			if (handler.fn(frame)) {
				handler_found = true;
				break;
			}
		}
	}

	if (!handler_found) {
		println("[kernel][x86]: warning: no handler found for irq ", num);
	}

	GIC->eoi(num);

	auto* cpu = get_current_thread()->cpu;
	for (auto& work : cpu->deferred_work) {
		cpu->deferred_work.remove(&work);
		work.fn();
	}
}
