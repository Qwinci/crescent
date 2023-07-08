#include "arch/misc.hpp"

[[noreturn, gnu::used]] void kmain() {
	while (true) {
		arch_hlt();
	}
}
