#include "gic_v3.hpp"
#include "gic.hpp"
#include "utils/driver.hpp"
#include "mem/iospace.hpp"
#include "vector.hpp"
#include "manually_destroy.hpp"
#include "sched/sched.hpp"
#include "arch/cpu.hpp"

namespace {
	IoSpace DIST_SPACE {};
	ManuallyDestroy<kstd::vector<IoSpace>> REDIST_SPACES {};
}

namespace {
	constexpr BitRegister<u32> GICD_CTLR {0};
	constexpr BitRegister<u32> GICD_TYPER {0x4};
	constexpr usize GICD_IGROUPR {0x80};
	constexpr usize GICD_ISENABLER {0x100};
	constexpr usize GICD_ICENABLER {0x180};
	constexpr usize GICD_IPRIORITYR {0x400};
	constexpr usize GICD_ICFGR {0xC00};
	constexpr usize GICD_IGRPMODR {0xD00};
	constexpr usize GICD_IROUTER {0x6100};
}

namespace {
	constexpr BitRegister<u64> GICR_TYPER {0x8};
	constexpr BitRegister<u32> GICR_WAKER {0x14};

	constexpr usize GICR_IRQ_OFFSET = 0x10000;
}

namespace gicd_ctlr {
	static constexpr BitField<u32, bool> ENABLE_GRP1 {0, 1};
	static constexpr BitField<u32, bool> ENABLE_GRP1A {1, 1};
	static constexpr BitField<u32, bool> ARE_NS {4, 1};
	static constexpr BitField<u32, bool> RWP {31, 1};
}

namespace gicd_typer {
	static constexpr BitField<u32, u8> IT_LINES_NUMBER {0, 5};
}

namespace gicd_irouter {
	static constexpr BitField<u64, u8> AFF0 {0, 8};
	static constexpr BitField<u64, u8> AFF1 {8, 8};
	static constexpr BitField<u64, u8> AFF2 {16, 8};
	static constexpr BitField<u64, u8> AFF3 {32, 8};
}

namespace gicr_typer {
	static constexpr BitField<u64, bool> LAST {4, 1};
	static constexpr BitField<u64, u32> AFFINITY {32, 32};
}

namespace gicr_waker {
	static constexpr BitField<u32, bool> PROCESSOR_SLEEP {1, 1};
	static constexpr BitField<u32, bool> CHILDREN_ASLEEP {2, 1};
}

static IoSpace& get_redist(u32 affinity) {
	for (auto& redist : *REDIST_SPACES) {
		if ((redist.load(GICR_TYPER) & gicr_typer::AFFINITY) == affinity) {
			return redist;
		}
	}
	panic("[kernel][aarch64]: gic redistributor not found for affinity ", affinity);
}

struct GicV3 : public Gic {
	void mask_irq(u32 irq, bool mask) override {
		auto bit = irq % 32;
		auto offset = irq / 32 * 4;

		IoSpace space;
		if (irq < 32) {
			auto cpu = get_current_thread()->cpu;
			space = get_redist(cpu->affinity).subspace(GICR_IRQ_OFFSET);
		}
		else {
			space = DIST_SPACE;
		}

		if (mask) {
			space.store(GICD_ICENABLER + offset, 1U << bit);
		}
		else {
			space.store(GICD_ISENABLER + offset, 1U << bit);
		}
	}

	void set_mode(u32 irq, TriggerMode mode) override {
		if (irq < 16) {
			return;
		}

		auto offset = irq / 16 * 4;
		auto bit_offset = irq % 16 * 2;

		auto group_offset = irq / 32 * 4;
		auto group_bit_offset = irq % 32;

		IoSpace space;
		if (irq < 32) {
			space = get_redist(get_current_thread()->cpu->affinity).subspace(GICR_IRQ_OFFSET);
		}
		else {
			space = DIST_SPACE;
		}

		u32 bit_value = mode == TriggerMode::Edge ? 0b10 : 0;

		auto value = space.load<u32>(GICD_ICFGR + offset);
		value &= ~(0b11 << bit_offset);
		value |= bit_value << bit_offset;
		space.store(GICD_ICFGR + offset, value);

		auto group = space.load<u32>(GICD_IGROUPR + group_offset);
		group |= 1U << group_bit_offset;
		space.store(GICD_IGROUPR + group_offset, group);

		auto group_mod = space.load<u32>(GICD_IGRPMODR + group_offset);
		group_mod &= ~(1U << group_bit_offset);
		space.store(GICD_IGRPMODR + group_offset, group_mod);
	}

	void set_priority(u32 irq, u8 priority) override {
		auto offset = irq / 4 * 4;
		auto bit_offset = irq % 4 * 8;

		IoSpace space;
		if (irq < 32) {
			auto cpu = get_current_thread()->cpu;
			space = get_redist(cpu->affinity).subspace(GICR_IRQ_OFFSET);
		}
		else {
			space = DIST_SPACE;
		}

		auto value = space.load<u32>(GICD_IPRIORITYR + offset);
		value &= ~(0xFF << bit_offset);
		value |= priority << bit_offset;
		space.store(GICD_IPRIORITYR + offset, value);
	}

	void set_affinity(u32 irq, u32 affinity) override {
		if (irq < 32) {
			return;
		}

		auto offset = (irq - 32) * 8;
		auto value = gicd_irouter::AFF0(affinity) |
			gicd_irouter::AFF1(affinity >> 8) |
			gicd_irouter::AFF2(affinity >> 16) |
			gicd_irouter::AFF3(affinity >> 24);
		DIST_SPACE.store(GICD_IROUTER + offset, value);
	}

	u32 ack() override {
		u64 iar1;
		asm volatile("mrs %0, icc_iar1_el1" : "=r"(iar1));
		u32 irq = iar1 & 0xFFFFFF;
		return irq;
	}

	void eoi(u32 irq) override {
		if (irq < 1020) {
			asm volatile("msr icc_eoir1_el1, %0" : : "r"(u64 {irq}));
		}
	}

	void send_ipi(u32 affinity, u8 id) override {
		assert(id <= 0b1111);

		u64 sgi1r = 0;
		// target list
		sgi1r |= 1U << (affinity & 0xFF);
		// aff1
		sgi1r |= (affinity >> 8 & 0xFF) << 16;
		// intid
		sgi1r |= id << 24;
		// aff2
		sgi1r |= static_cast<u64>(affinity >> 16 & 0xFF) << 32;
		// aff3
		sgi1r |= static_cast<u64>(affinity >> 24 & 0xFF) << 48;
		asm volatile("msr icc_sgi1r_el1, %0" : : "r"(sgi1r));
	}

	void send_ipi_to_others(u8 id) override {
		assert(id <= 0b1111);

		u64 sgi1r = 0;
		// irm
		sgi1r |= u64 {1} << 40;
		sgi1r |= id << 24;
		asm volatile("msr icc_sgi1r_el1, %0" : : "r"(sgi1r));
	}

	void init_on_cpu() {
		auto& space = get_redist(get_current_thread()->cpu->affinity);
		auto waker = space.load(GICR_WAKER);
		waker &= ~gicr_waker::PROCESSOR_SLEEP;
		space.store(GICR_WAKER, waker);
		while (space.load(GICR_WAKER) & gicr_waker::CHILDREN_ASLEEP);

		space.store(GICR_IRQ_OFFSET + GICD_IGROUPR, ~0U);
		space.store(GICR_IRQ_OFFSET + GICD_IGRPMODR, 0U);

		// enable cpu interface
		u64 sre;
		asm volatile("mrs %0, icc_sre_el1" : "=r"(sre));
		sre |= 1;
		asm volatile("msr icc_sre_el1, %0" : : "r"(sre));

		// configure the priority mask level
		u64 pmr = 0xFF;
		asm volatile("msr icc_pmr_el1, %0" : : "r"(pmr));

		// configure pre-emption to disabled
		u64 bpr = 0b111;
		asm volatile("msr icc_bpr1_el1, %0" : : "r"(bpr));

		u64 cpu_ctlr;
		asm volatile("mrs %0, icc_ctlr_el1" : "=r"(cpu_ctlr));
		// eoir does both priority drop and deactivation
		cpu_ctlr &= ~(1U << 1);
		asm volatile("msr icc_ctlr_el1, %0" : : "r"(cpu_ctlr));

		// enable group 1 irqs
		u64 igrpen1;
		asm volatile("mrs %0, icc_igrpen1_el1" : "=r"(igrpen1));
		igrpen1 |= 1;
		asm volatile("msr icc_igrpen1_el1, %0" : : "r"(igrpen1));

		for (u32 i = 0; i < 32; ++i) {
			mask_irq(i, true);
			set_priority(i, 0);
			if (i < 16) {
				mask_irq(i, false);
			}
		}
	}
};

ManuallyDestroy<GicV3> GIC_V3 {};

void gic_v3_init_on_cpu() {
	GIC_V3->init_on_cpu();
}

void aarch64_irq_init();

static InitStatus gic_v3_init(DtbNode& node) {
	println("[kernel][aarch64]: gic v3 init");
	GIC = &*GIC_V3;
	GIC_VERSION = 3;

	auto [dist_phys, dist_size] = node.regs[0];
	auto [redist_phys, redist_size] = node.regs[1];

	DIST_SPACE.set_phys(dist_phys, dist_size);

	IoSpace redist_space {redist_phys, redist_size};
	assert(redist_space.map(CacheMode::Uncached));

	auto redist_count = redist_size / 0x20000;
	REDIST_SPACES->reserve(redist_count);

	for (usize i = 0; i < redist_count; ++i) {
		auto space = redist_space.subspace(i * 0x20000);
		REDIST_SPACES->push(space);

		if (space.load(GICR_TYPER) & gicr_typer::LAST) {
			break;
		}
	}

	assert(DIST_SPACE.map(CacheMode::Uncached));

	DIST_SPACE.store(
		GICD_CTLR,
		DIST_SPACE.load(GICD_CTLR) |
		gicd_ctlr::ENABLE_GRP1A(true) |
		gicd_ctlr::ARE_NS(true));
	while (DIST_SPACE.load(GICD_CTLR) & gicd_ctlr::RWP);

	u32 irq_lines = DIST_SPACE.load(GICD_TYPER) & gicd_typer::IT_LINES_NUMBER;
	u32 num_irqs = kstd::min((irq_lines + 1) * 32, 1020U);
	GIC_V3->num_irqs = num_irqs;

	auto affinity = get_current_thread()->cpu->affinity;

	for (u32 i = 32; i < num_irqs; ++i) {
		GIC_V3->mask_irq(i, true);
		GIC_V3->set_priority(i, 0);
		GIC_V3->set_affinity(i, affinity);
	}

	gic_init_on_cpu();
	aarch64_irq_init();

	return InitStatus::Success;
}

static DtDriver GIC_V3_DRIVER {
	.init = gic_v3_init,
	.compatible {"arm,gic-v3"},
	.provides {"gic"}
};

DT_DRIVER(GIC_V3_DRIVER);
