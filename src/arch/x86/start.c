[[noreturn]] extern void kmain();

extern void x86_init_con();
extern void x86_init_mem();

[[noreturn, gnu::used]] void start() {
	x86_init_con();
	x86_init_mem();
	kmain();
}