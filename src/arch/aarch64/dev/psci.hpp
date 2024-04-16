#pragma once
#include "types.hpp"

enum class PsciError : i32 {
	Success = 0,
	NotSupported = -1,
	InvalidParameters = -2,
	Denied = -3,
	AlreadyOn = -4,
	OnPending = -5,
	InternalFailure = -6,
	NotPresent = -7,
	Disabled = -8,
	InvalidAddress = -9
};

u64 psci_call(u64 a0);
u64 psci_call(u64 a0, u64 a1);
u64 psci_call(u64 a0, u64 a1, u64 a2);
u64 psci_call(u64 a0, u64 a1, u64 a2, u64 a3);

u32 psci_version();
i32 psci_cpu_on(u64 target_mpidr, u64 entry_phys, u64 ctx);
void psci_system_off();
void psci_system_reset();
