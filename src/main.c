#include "arch/misc.h"
#include "mem/pmalloc.h"
#include "stdio.h"

[[noreturn]] void kmain() {
	kprintf("[kernel]: entered main\n");

	Page* first = pmalloc(2);
	Page* second = pmalloc(2);
	kprintf("%p\n", first);
	kprintf("%p\n", second);
	kprintf(first->phys + 0x2000 == second->phys ? "true\n" : "false\n");
	pfree(first, 2);
	pfree(second, 2);
	while (true) {
		arch_hlt();
	}
}