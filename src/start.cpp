#include "acpi/common.hpp"
#include "acpi/lapic.hpp"
#include "arch.hpp"
#include "console.hpp"
#include "cpu/cpu.hpp"
#include "noalloc/string.hpp"
#include "sched/sched.hpp"
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

[[noreturn]] void ap_entry(u8 id, u32 acpi_id) {
	//println("hello from cpu ", id);
	while (true) asm("hlt");
}

[[noreturn]] void test_task();

extern "C" [[noreturn, gnu::used]] void kstart() {
	for (fn* f = __init_array_start; f != __init_array_end; ++f) {
		(*f)();
	}

	auto rsdp = arch_get_rsdp();
	auto font = arch_get_font();
	auto fb = arch_get_framebuffer();

	init_console(&fb, font);
	set_fg(0x00FF00);

	arch_init_mem();

	auto data = new CpuLocal();
	load_gdt(&data->tss);
	set_cpu_local(data);
	asm volatile("mov ax, 0x28; ltr ax" : : : "ax");
	set_exceptions();
	load_idt();
	enable_interrupts();

	init_timers(rsdp);
	parse_madt(locate_acpi_table(rsdp, "APIC"));
	Lapic::calibrate_timer();

	arch_init_smp(ap_entry);

	register_int_handler(timer_vec, timer_int);

	println("testing scheduler");
	//test_sched();

	sched_init();

	//start_timer();

	auto task = create_kernel_task("test task", test_task);
	sched_queue_task(task);
	sched();
	println("back in kernel");
	sched();
	println("back again");

	while (true) {
		asm("hlt");
	}
}