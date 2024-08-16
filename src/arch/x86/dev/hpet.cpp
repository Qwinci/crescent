#include "hpet.hpp"
#include "acpi/acpi.hpp"
#include "mem/iospace.hpp"
#include "dev/clock.hpp"
#include "manually_init.hpp"

struct [[gnu::packed]] HpetTable {
	acpi::SdtHeader hdr;
	u8 hw_rev_id;
	u8 flags;
	u16 pci_vendor_id;
	acpi::Address address;
	u8 hpet_num;
	u16 min_tick;
	u8 page_protection;
};

namespace regs {
	constexpr BitRegister<u64> GCIDR {0};
	constexpr BitRegister<u64> GCR {0x10};
	constexpr BasicRegister<u64> MCVR {0xF0};
}

namespace gcidr {
	constexpr BitField<u64, u32> COUNTER_CLK_PERIOD {32, 32};
}

namespace gcr {
	constexpr BitField<u64, bool> ENABLE_CNF {0, 1};
}

namespace {
	IoSpace SPACE;
}

struct Hpet final : ClockSource {
	constexpr explicit Hpet(usize freq_in_us) : ClockSource {"hpet", freq_in_us} {}

	u64 get() override {
		return SPACE.load(regs::MCVR);
	}
};

namespace {
	ManuallyInit<Hpet> HPET;
}

void hpet_init() {
	auto* table = static_cast<const HpetTable*>(acpi::get_table("HPET"));
	if (!table) {
		return;
	}

	assert(table->address.is_memory());
	SPACE.set_phys(table->address.address, 0x1000);
	assert(SPACE.map(CacheMode::Uncached));

	auto fs_in_tick = SPACE.load(regs::GCIDR) & gcidr::COUNTER_CLK_PERIOD;
	auto ns_in_tick = fs_in_tick / 1000000;
	auto ticks_in_us = 1000 / ns_in_tick;

	SPACE.store(regs::GCR, gcr::ENABLE_CNF(true));
	HPET.initialize(ticks_in_us);
	clock_source_register(&*HPET);
}
