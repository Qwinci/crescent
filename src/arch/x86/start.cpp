#include "acpi/acpi.hpp"
#include "arch/x86/dev/hpet.hpp"
#include "arch/x86/dev/ps2.hpp"
#include "assert.hpp"
#include "cpu.hpp"
#include "dev/random.hpp"
#include "loader/info.hpp"
#include "mod.hpp"
#include "smp.hpp"

[[noreturn]] void kmain(const void* initrd, usize initrd_size);

extern void x86_madt_parse();

extern "C" [[noreturn, gnu::used]] void arch_start(BootInfo info) {
	acpi::init(info.rsdp);
	hpet_init();
	random_initialize();
	x86_smp_init();
	x86_madt_parse();
	x86_ps2_init();

	Module initrd {};
	assert(x86_get_module(initrd, "initramfs.tar"));

	if (CPU_FEATURES.rdseed) {
		u64 seed;
		asm volatile("0: rdseed %0; jnc 0b" : "=r"(seed));
		random_add_entropy(&seed, 1);
		println("[kernel][x86]: initial entropy added from rdseed");
	}
	else if (CPU_FEATURES.rdrnd) {
		u64 seed;
		asm volatile("0: rdrand %0; jnc 0b" : "=r"(seed));
		random_add_entropy(&seed, 1);
		println("[kernel][x86]: initial entropy added from rdrand");
	}

	kmain(initrd.data, initrd.size);
}
