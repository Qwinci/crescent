#include "acpi/common.hpp"
#include "acpi/lapic.hpp"
#include "arch.hpp"
#include "console.hpp"
#include "cpu/cpu.hpp"
#include "noalloc/string.hpp"
#include "timer/timer.hpp"
#include "timer/timer_int.hpp"
#include "types.hpp"

[[gnu::used]] u8 stack[0x2000];

extern "C" [[noreturn, gnu::naked, gnu::used]] void start() {
	asm volatile("lea rsp, [stack + 0x2000]; jmp kstart");
}

using fn = void (*)();

extern "C" fn __init_array_start[]; // NOLINT(bugprone-reserved-identifier)
extern "C" fn __init_array_end[]; // NOLINT(bugprone-reserved-identifier)

extern "C" [[noreturn, gnu::used]] void kstart() {
	for (fn* f = __init_array_start; f != __init_array_end; ++f) {
		(*f)();
	}

	arch_init_mem();

	auto rsdp = arch_get_rsdp();
	auto font = arch_get_font();
	auto fb = arch_get_framebuffer();

	init_console(&fb, font);

	set_fg(0x00FF00);

	println("creating cpu local");
	auto data = new CpuLocal();
	println("cpu local created, loading gdt");
	load_gdt(data->gdt, 7);
	println("gdt loaded");
	set_cpu_local(data);
	asm volatile("mov ax, 0x28; ltr ax" : : : "ax");
	println("enabling interrupts");
	set_exceptions();
	load_idt(&data->idt);
	enable_interrupts();

	init_timers(rsdp);

	parse_madt(locate_acpi_table(rsdp, "APIC"));

	Lapic::calibrate_timer();

	register_int_handler(timer_vec, timer_int);

	println("hello");

	while (true) {
		asm("hlt");
	}
}