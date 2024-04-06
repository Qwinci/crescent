target_sources(crescent PRIVATE
	gdt.cpp
	gdt.S
	idt.cpp
	stubs.S
	irq.cpp
	exceptions.cpp
	usermode.S
)
