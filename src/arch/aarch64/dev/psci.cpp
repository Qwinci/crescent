#include "psci.hpp"
#include "utils/driver.hpp"

namespace {
	bool USE_SMC = false;
	bool INITIALIZED = false;
}

u64 psci_call(u64 a0) {
	assert(INITIALIZED);

	register u64 x0 asm("x0") = a0;
	register u64 out asm("x0");
	if (USE_SMC) {
		asm volatile("smc #0" : "=r"(out) : "r"(x0));
	}
	else {
		asm volatile("hvc #0" : "=r"(out) : "r"(x0));
	}
	return out;
}

u64 psci_call(u64 a0, u64 a1) {
	assert(INITIALIZED);

	register u64 x0 asm("x0") = a0;
	register u64 x1 asm("x1") = a1;
	register u64 out asm("x0");
	if (USE_SMC) {
		asm volatile("smc #0" : "=r"(out) : "r"(x0), "r"(x1));
	}
	else {
		asm volatile("hvc #0" : "=r"(out) : "r"(x0), "r"(x1));
	}
	return out;
}

u64 psci_call(u64 a0, u64 a1, u64 a2) {
	assert(INITIALIZED);

	register u64 x0 asm("x0") = a0;
	register u64 x1 asm("x1") = a1;
	register u64 x2 asm("x2") = a2;
	register u64 out asm("x0");
	if (USE_SMC) {
		asm volatile("smc #0" : "=r"(out) : "r"(x0), "r"(x1), "r"(x2));
	}
	else {
		asm volatile("hvc #0" : "=r"(out) : "r"(x0), "r"(x1), "r"(x2));
	}
	return out;
}

u64 psci_call(u64 a0, u64 a1, u64 a2, u64 a3) {
	assert(INITIALIZED);

	register u64 x0 asm("x0") = a0;
	register u64 x1 asm("x1") = a1;
	register u64 x2 asm("x2") = a2;
	register u64 x3 asm("x3") = a3;
	register u64 out asm("x0");
	if (USE_SMC) {
		asm volatile("smc #0" : "=r"(out) : "r"(x0), "r"(x1), "r"(x2), "r"(x3));
	}
	else {
		asm volatile("hvc #0" : "=r"(out) : "r"(x0), "r"(x1), "r"(x2), "r"(x3));
	}
	return out;
}

u32 psci_version() {
	return psci_call(0x84000000);
}

i32 psci_cpu_on(u64 target_mpidr, u64 entry_phys, u64 ctx) {
	return static_cast<i32>(psci_call(0xC4000003, target_mpidr, entry_phys, ctx));
}

void psci_system_off() {
	psci_call(0x84000008);
}

void psci_system_reset() {
	psci_call(0x84000009);
}

static bool psci_init(dtb::Node node, dtb::Node) {
	auto method_opt = node.prop("method");
	if (!method_opt) {
		panic("psci node contains no method");
	}

	kstd::string_view method {static_cast<const char*>(method_opt->ptr)};
	if (method == "smc") {
		USE_SMC = true;
	}
	else if (method == "hvc") {
		USE_SMC = false;
	}
	else {
		panic("Unknown psci method: ", method);
	}

	INITIALIZED = true;

	return true;
}

static DtDriver PSCI_DRIVER {
	.init = psci_init,
	.compatible {"arm,psci-1.0"}
};

DT_DRIVER(PSCI_DRIVER);
