extern "C" void start() {
	while (true) {
		asm("hlt");
	}
}