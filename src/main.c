#include "arch/misc.h"
#include "stdio.h"

[[noreturn]] void kmain() {
	kprintf("[kernel]: entered main\n");
	while (true) {
		arch_hlt();
	}
}