#include "crescent/syscall.h"
#include "crescent/syscalls.h"
#include "../../libc/include/elf.h"

int main();

using Fn = void (*)();

extern Fn __init_array_start[];
extern Fn __init_array_end[];
extern Fn __fini_array_start[];
extern Fn __fini_array_end[];

[[gnu::visibility("hidden")]] extern Elf64_Dyn _DYNAMIC[];
[[gnu::visibility("hidden")]] extern uintptr_t _GLOBAL_OFFSET_TABLE_[];

#ifdef __x86_64__
#define R_RELATIVE R_X86_64_RELATIVE
#elif defined(__aarch64__)
#define R_RELATIVE R_AARCH64_RELATIVE
#else
#error Unsupported architecture
#endif

extern "C" void _start() {
	auto runtime_dyn = reinterpret_cast<uintptr_t>(_DYNAMIC);
	auto link_dyn = _GLOBAL_OFFSET_TABLE_[0];
	uintptr_t base = runtime_dyn - link_dyn;

	uintptr_t rela_addr = 0;
	size_t rela_size = 0;

	for (auto* dyn = _DYNAMIC; dyn->d_tag != DT_NULL; ++dyn) {
		if (dyn->d_tag == DT_RELA) {
			rela_addr = base + dyn->d_un.d_ptr;
		}
		else if (dyn->d_tag == DT_RELASZ) {
			rela_size = dyn->d_un.d_val;
		}
	}

	for (uintptr_t i = 0; i < rela_size; i += sizeof(Elf64_Rela)) {
		auto* rela = reinterpret_cast<Elf64_Rela*>(rela_addr + i);
		if (ELF64_R_TYPE(rela->r_info) == R_RELATIVE) {
			auto* ptr = reinterpret_cast<uintptr_t*>(base + rela->r_offset);
			*ptr = base + rela->r_addend;
		}
	}

	for (auto* fn = __init_array_start; fn != __init_array_end; ++fn) {
		(*fn)();
	}

	int status = main();

	for (auto* fn = __fini_array_start; fn != __fini_array_end; ++fn) {
		(*fn)();
	}
	syscall(SYS_PROCESS_EXIT, status);
	__builtin_trap();
}
