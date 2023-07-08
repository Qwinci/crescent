#include "mem/mem.hpp"

using Fn = void (*)();
extern Fn __init_array_start[]; // NOLINT(bugprone-reserved-identifier)
extern Fn __init_array_end[]; // NOLINT(bugprone-reserved-identifier)

extern "C" [[noreturn, gnu::used]] void kstart() {
	x86_init_mem();
	for (auto* fn = __init_array_start; fn != __init_array_end; ++fn) {
		(*fn)();
	}

	while (true) {
		asm volatile("hlt");
	}
}