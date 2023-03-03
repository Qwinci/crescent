#include "acpi/common.hpp"
#include "acpi/fadt.hpp"
#include "arch.hpp"
#include "arch/x86/lapic.hpp"
#include "console.hpp"
#include "cpu/cpu.hpp"
#include "debug/backtrace.hpp"
#include "drivers/pci.hpp"
#include "drivers/ps2.hpp"
#include "sched/sched.hpp"
#include "timer/timer.hpp"
#include "timer/timer_int.hpp"
#include "types.hpp"

[[gnu::used]] u8 stack[0x2000];

extern "C" [[noreturn, gnu::naked, gnu::used]] void start() {
	asm volatile("lea rsp, [stack + 0x2000]; xor rbp, rbp; call kstart");
}

using fn = void (*)();

extern "C" fn __init_array_start[]; // NOLINT(bugprone-reserved-identifier)
extern "C" fn __init_array_end[]; // NOLINT(bugprone-reserved-identifier)

[[noreturn]] void ap_entry(u8 id, u32 acpi_id) {
	arch_init_usermode();
	sched_init(false);
	start_timer();
	while (true) {
		arch_hlt();
	}
}

extern "C" [[noreturn, gnu::used]] void kstart() {
	for (fn* f = __init_array_start; f != __init_array_end; ++f) {
		(*f)();
	}

	auto rsdp = arch_get_rsdp();
	auto font = arch_get_font();

	init_console(&current_fb, font);
	set_fg(0x00FF00);

	arch_init_mem();

	arch_init_cpu_locals();
	full_int_init = true;

	init_timers(rsdp);
	parse_madt(locate_acpi_table(rsdp, "APIC"));
	Lapic::calibrate_timer();

	register_int_handler(timer_vec, timer_int);

	arch_init_smp(ap_entry);

	arch_init_usermode();

	init_ps2();
	init_fadt(rsdp);

	/*auto usertest_file = arch_get_module("usertest");

	auto* hdr = (ElfEHdr*) usertest_file;
	if (!hdr || hdr->e_ident[EI_MAG0] != ELFMAG0 ||
		hdr->e_ident[EI_MAG1] != ELFMAG1 ||
		hdr->e_ident[EI_MAG2] != ELFMAG2 ||
		hdr->e_ident[EI_MAG3] != ELFMAG3 ) {
		panic("invalid usertest file");
	}

	usize in_mem_size = 0;
	usize base = 0;
	bool base_found = false;
	for (u16 i = 0; i < hdr->e_phnum; ++i) {
		auto phdr = (ElfPHdr*) ((usize) hdr + hdr->e_phoff + i * hdr->e_phentsize);
		if (phdr->p_type == PT_LOAD) {
			if (!base_found) {
				base = phdr->p_vaddr;
				base_found = true;
			}
			in_mem_size += ALIGNUP(phdr->p_memsz, phdr->p_align - 1);
		}
	}

	vm_user_init(HHDM_OFFSET);

	get_map()->ensure_toplevel_entries();
	auto map = get_map()->create_high_half_shared();

	auto user_mem = vm_user_alloc_backed(map, (in_mem_size + 0x1000 - 1) / 0x1000, AllocFlags::Rw | AllocFlags::Exec);
	auto k_user_mem = vm_user_alloc_kernel_mapping(map, user_mem, (in_mem_size + 0x1000 - 1) / 0x1000);

	for (u16 i = 0; i < hdr->e_phnum; ++i) {
		auto phdr = (ElfPHdr*) ((usize) hdr + hdr->e_phoff + i * hdr->e_phentsize);
		if (phdr->p_type == PT_LOAD) {
			const void* src = (const void*) ((usize) hdr + phdr->p_offset);
			memcpy((void*) ((usize) k_user_mem + (phdr->p_vaddr - base)), src, phdr->p_filesz);
		}
	}

	vm_user_dealloc_kernel_mapping(k_user_mem, (in_mem_size + 0x1000 - 1) / 0x1000);

	auto user_entry = (void (*)()) ((usize) user_mem + (hdr->e_entry - base));*/

	sched_init(true);

	start_timer();

	init_pci(rsdp);

	while (true) {
		arch_hlt();
	}
}