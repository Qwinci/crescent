#include "acpi/acpi.hpp"
#include "arch/x86/dev/hpet.hpp"
#include "loader/info.hpp"
#include "mod.hpp"
#include "smp.hpp"
#include "assert.hpp"

[[noreturn]] void kmain(const void* initrd, usize initrd_size);

extern "C" [[noreturn, gnu::used]] void arch_start(BootInfo info) {
	acpi::init(info.rsdp);
	hpet_init();
	x86_smp_init();

	Module initrd {};
	assert(x86_get_module(initrd, "initramfs.tar"));

	kmain(initrd.data, initrd.size);
}
