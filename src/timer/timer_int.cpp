#include "timer_int.hpp"
#include "acpi/lapic.hpp"
#include "console.hpp"

u16 timer_vec {};

void timer_int(InterruptCtx*) {
	println("timer int");
	Lapic::eoi();
}