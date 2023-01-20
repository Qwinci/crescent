#include "interrupts.hpp"

struct [[gnu::packed]] Idtr {
	u16 size;
	u64 offset;
};

Idt* current_idt;

void load_idt(Idt* idt) {
	asm volatile("cli");
	Idtr idtr {.size = 0x1000 - 1, .offset = cast<u64>(idt)};
	asm volatile("lidt %0" : : "m"(idtr));
	asm volatile("sti");
	current_idt = idt;
}