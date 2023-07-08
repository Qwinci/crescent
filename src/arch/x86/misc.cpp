void arch_hlt() {
	asm volatile("hlt" : : : "memory");
}