#include "arch/irq.hpp"
#include "x86/irq.hpp"
#include "stdio.hpp"
#include "arch/x86/dev/lapic.hpp"
#include "arch/cpu.hpp"

static u32 USED_IRQS[256 / 32] {};
static u32 USED_SHAREABLE_IRQS[256 / 32] {};

u32 x86_alloc_irq(u32 count, bool shared) {
	if (!count) {
		return 0;
	}

	for (u32 i = 32; i < 256; ++i) {
		bool found = true;
		for (u32 j = i; j < i + count; ++j) {
			if ((USED_IRQS[j / 32] & 1U << (j % 32)) || (!shared && USED_SHAREABLE_IRQS[j / 32] & 1U << (j % 32))) {
				found = false;
				break;
			}
		}

		if (found) {
			if (shared) {
				for (u32 j = i; j < i + count; ++j) {
					USED_SHAREABLE_IRQS[j / 32] |= 1U << (j % 32);
				}
			}
			else {
				for (u32 j = i; j < i + count; ++j) {
					USED_IRQS[j / 32] |= 1U << (j % 32);
				}
			}
			return i;
		}
	}

	return 0;
}

void x86_dealloc_irq(u32 irq, u32 count, bool shared) {
	if (shared) {
		for (u32 i = irq; i < irq + count; ++i) {
			USED_SHAREABLE_IRQS[i / 32] &= ~(1U << (i % 32));
		}
	}
	else {
		for (u32 i = irq; i < irq + count; ++i) {
			USED_IRQS[i / 32] &= ~(1U << (i % 32));
		}
	}
}

static DoubleList<IrqHandler, &IrqHandler::hook> IRQ_HANDLERS[256] {};
static Spinlock<void> IRQ_HANDLERS_LOCK {};

void register_irq_handler(u32 num, IrqHandler* handler) {
	IrqGuard irq_guard {};
	auto guard = IRQ_HANDLERS_LOCK.lock();
	if (!IRQ_HANDLERS[num].is_empty()) {
		if (!IRQ_HANDLERS[num].front()->can_be_shared || !handler->can_be_shared) {
			return;
		}
	}
	IRQ_HANDLERS[num].push(handler);
}

void deregister_irq_handler(u32 num, IrqHandler* handler) {
	IrqGuard irq_guard {};
	auto guard = IRQ_HANDLERS_LOCK.lock();
	if (IRQ_HANDLERS[num].is_empty()) {
		return;
	}
	IRQ_HANDLERS[num].remove(handler);
}

extern "C" [[gnu::used]] void arch_irq_handler(IrqFrame* frame, u8 num) {
	if (frame->cs == 0x2B) {
		asm volatile("swapgs");
	}

	lapic_eoi();

	bool handler_found = false;
	{
		auto guard = IRQ_HANDLERS_LOCK.lock();
		for (auto& handler : IRQ_HANDLERS[num]) {
			if (handler.fn(frame)) {
				handler_found = true;
				break;
			}
		}
	}

	if (!handler_found) {
		println("[kernel][x86]: warning: no handler found for irq ", num);
	}

	auto* cpu = get_current_thread()->cpu;
	for (auto& work : cpu->deferred_work) {
		cpu->deferred_work.remove(&work);
		work.fn();
	}

	if (frame->cs == 0x2B) {
		asm volatile("swapgs");
	}
}
