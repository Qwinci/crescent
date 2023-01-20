#include "interrupts.hpp"

struct [[gnu::packed]] Idtr {
	u16 size;
	u64 offset;
};

void load_idt(Idt* idt) {
	asm volatile("cli");
	Idtr idtr {.size = 0x1000 - 1, .offset = cast<u64>(idt)};
	asm volatile("lidt %0" : : "m"(idtr));
	asm volatile("sti");
}