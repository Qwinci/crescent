#include "acpi/acpi.hpp"
#include "arch/x86/dev/hpet.hpp"
#include "arch/x86/dev/ps2.hpp"
#include "arch/x86/dev/rtc.hpp"
#include "assert.hpp"
#include "cpu.hpp"
#include "dev/random.hpp"
#include "loader/info.hpp"
#include "mod.hpp"
#include "smp.hpp"

[[noreturn]] void kmain(const void* initrd, usize initrd_size);

extern void x86_madt_parse();

u64 arch_get_random_seed() {
	if (CPU_FEATURES.rdseed) {
		u64 seed;
		asm volatile("0: rdseed %0; jnc 0b" : "=r"(seed));
		return seed;
	}
	else {
		u32 tsc_low;
		u32 tsc_high;
		asm volatile("rdtsc" : "=a"(tsc_low), "=d"(tsc_high));
		return static_cast<u64>(tsc_high) << 32 | tsc_low;
	}
}

extern "C" [[noreturn, gnu::used]] void arch_start(BootInfo info) {
	acpi::init(info.rsdp);
	acpi::sleep_init();
	hpet_init();
	x86_smp_init();
	x86_madt_parse();
	x86_ps2_init();
	x86_rtc_init();

	Module initrd {};
	assert(x86_get_module(initrd, "initramfs.tar"));

	if (CPU_FEATURES.rdseed) {
		u64 data[] {
			arch_get_random_seed(),
			arch_get_random_seed(),
			arch_get_random_seed(),
			arch_get_random_seed()
		};
		random_add_entropy(data, 4, 256);
		println("[kernel][x86]: initial entropy added from rdseed");
	}
	else if (CPU_FEATURES.rdrnd) {
		auto get_seed = []() {
			u64 seed;
			asm volatile("0: rdrand %0; jnc 0b" : "=r"(seed));
			return seed;
		};

		u64 data[] {
			get_seed(),
			get_seed(),
			get_seed(),
			get_seed()
		};
		random_add_entropy(data, 4, 256);

		println("[kernel][x86]: initial entropy added from rdrand");
	}

	kmain(initrd.data, initrd.size);
}
