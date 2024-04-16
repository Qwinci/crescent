#include "acpi/acpi.hpp"
#include "arch/x86/dev/hpet.hpp"
#include "arch/x86/dev/ps2.hpp"
#include "assert.hpp"
#include "loader/info.hpp"
#include "mod.hpp"
#include "smp.hpp"

[[noreturn]] void kmain(const void* initrd, usize initrd_size);

extern void x86_madt_parse();

extern "C" [[noreturn, gnu::used]] void arch_start(BootInfo info) {
	acpi::init(info.rsdp);
	hpet_init();
	x86_smp_init();
	x86_madt_parse();
	x86_ps2_init();

	Module initrd {};
	assert(x86_get_module(initrd, "initramfs.tar"));

	kmain(initrd.data, initrd.size);
}
