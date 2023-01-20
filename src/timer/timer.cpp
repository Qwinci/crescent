#include "timer.hpp"
#include "acpi/common.hpp"
#include "console.hpp"
#include "drivers/hpet.hpp"
#include "drivers/tsc.hpp"
#include "utils/cpuid.hpp"

void (*udelay_ptr)(u64 us);

static u64 tsc_ticks_in_sec = 0;

void init_timers(const void* rsdp) {
	bool hpet_initialized;
	if (auto hpet = locate_acpi_table(rsdp, "HPET")) {
		hpet_initialized = initialize_hpet(hpet);
		if (hpet_initialized) {
			udelay_ptr = hpet_sleep;
		}
	}

	if (!udelay_ptr) {
		panic("error: no timers are available");
	}

	auto max_supported_extended_leaf = cpuid(0x80000000);
	udelay_ptr = hpet_sleep;
	if (max_supported_extended_leaf.eax >= 0x80000007) {
		auto inv_tsc_cpuid = cpuid(0x80000007);
		if ((inv_tsc_cpuid.edx & 1 << 8) == 0) {
			return;
		}

		if (!udelay_ptr) {
			panic("error: invariant tsc is supported but no other timers are available");
		}

		println("info: performing tsc calibration");

		auto start = read_timestamp();
		udelay(10 * 1000);
		auto end = read_timestamp();

		auto diff = end - start;
		tsc_ticks_in_sec = diff * 100;
		println("info: tsc frequency: ", tsc_ticks_in_sec, "hz");

		udelay_ptr = [](u64 us) {
			auto start = read_timestamp();
			auto end = start + tsc_ticks_in_sec / 1000 / 1000;
			while (read_timestamp() < end);
		};
		println("info: using tsc for udelay");
	}
}
