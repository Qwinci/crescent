#pragma once
#include "expected.hpp"
#include "mem/pmalloc.hpp"
#include "memory.hpp"
#include "shared_ptr.hpp"
#include "utils/flags_enum.hpp"
#include "crescent/evm.h"

namespace evm {
	struct VirtualCpu {
		virtual ~VirtualCpu() = default;

		virtual int run() = 0;
		virtual int write_state(EvmStateBits changed_state) = 0;
		virtual int read_state(EvmStateBits wanted_state) = 0;
		virtual int trigger_irq(const EvmIrqInfo& info) = 0;

		u64 state_phys {};
		EvmGuestState* state {};
	};

	struct Evm {
		virtual ~Evm();

		virtual int map_page(u64 guest, u64 host) = 0;
		virtual void unmap_page(u64 guest) = 0;

		virtual kstd::expected<kstd::shared_ptr<VirtualCpu>, int> create_vcpu() = 0;

		DoubleList<Page, &Page::hook> pages {};
	};
}
