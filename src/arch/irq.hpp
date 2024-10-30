#pragma once
#include "arch/arch_irq.hpp"
#include "functional.hpp"
#include "double_list.hpp"

struct IrqHandler {
	DoubleListHook hook {};
	kstd::small_function<bool(IrqFrame* frame)> fn {};
	bool can_be_shared {};
};

void register_irq_handler(u32 num, IrqHandler* handler);
void deregister_irq_handler(u32 num, IrqHandler* handler);

enum class Ipi {
	Halt,
	Max
};

struct Cpu;

void arch_send_ipi(Ipi ipi, Cpu* cpu);
