#include "timer_int.hpp"
#include "console.hpp"

[[gnu::interrupt]] void timer_int(InterruptFrame*) {
	println("timer int");
}