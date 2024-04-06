#include "utils/driver.hpp"
#include "mem/iospace.hpp"

static IoSpace DIST_SPACE {};
static IoSpace REDIST_SPACE {};

constexpr BasicRegister<uint32_t> GICD_CTLR {0};
constexpr usize GICD_IGROUPR {0x80};
constexpr usize GICD_ISENABLER {0x100};
constexpr usize GICD_ICENABLER {0x180};
constexpr usize GICD_IPRIORITYR {0x400};
constexpr usize GICD_ICFGR {0xC00};
constexpr usize GICD_IGRPMODR {0xD00};
constexpr usize GICD_IROUTER {0x6100};

constexpr BasicRegister<uint32_t> GICR_WAKER {0x14};
constexpr usize GICR_IGROUP {0x10080};
constexpr usize GICR_ISENABLER {0x10100};
constexpr usize GICR_ICENABLER {0x10180};
constexpr usize GICR_IPRIORITYR {0x10400};
constexpr usize GICR_ICFGR {0x10C00};
constexpr usize GICR_IGRPMODR {0x10D00};

void config_irq(u32 irq, bool edge) {
	u32 priority_offset = irq / 4 * 4;
	u32 config_offset = irq / 16 * 4;
	u32 enable_offset = irq / 32 * 4;

	if (irq < 32) {
		// 0x00 == highest priority, 0xFF == lowest possible
		auto priority = REDIST_SPACE.load<u32>(GICR_IPRIORITYR + priority_offset);
		priority &= ~(0xFF << (irq % 4 * 8));
		// highest priority
		priority |= 0x00 << (irq % 4 * 8);
		REDIST_SPACE.store(GICR_IPRIORITYR + priority_offset, priority);

		auto config = REDIST_SPACE.load<u32>(GICR_ICFGR + config_offset);
		config &= ~(0b11 << (irq % 16 * 2));
		config |= (edge ? 0b10 : 0b00) << (irq % 16 * 2);
		REDIST_SPACE.store(GICR_ICFGR + config_offset, config);

		REDIST_SPACE.store(GICR_ISENABLER + enable_offset, 1U << (irq % 32));

		REDIST_SPACE.store<u32>(GICR_IGROUP, ~0);
		REDIST_SPACE.store<u32>(GICR_IGRPMODR, 0);

		/*u64 mpidr;
		asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
		u32 affinity = (mpidr & 0xFFFFFF) | (mpidr >> 32 & 0xFF) << 24;

		u64 sgi1r = 0;
		// target list
		sgi1r |= 1U << (affinity & 0xFF);
		// aff1
		sgi1r |= (affinity >> 8 & 0xFF) << 16;
		// intid
		sgi1r |= 16 << 24;
		// aff2
		sgi1r |= static_cast<u64>(affinity >> 16 & 0xFF) << 32;
		// aff3
		sgi1r |= static_cast<u64>(affinity >> 24 & 0xFF) << 48;
		asm volatile("msr icc_sgi1r_el1, %0" : : "r"(sgi1r));*/
	}
	else {
		auto priority = DIST_SPACE.load<u32>(GICD_IPRIORITYR + priority_offset);
		priority &= ~(0xFF << (irq % 4 * 8));
		// highest priority
		priority |= 0x00 << (irq % 4 * 8);
		DIST_SPACE.store(GICD_IPRIORITYR + priority_offset, priority);

		auto config = DIST_SPACE.load<u32>(GICD_ICFGR + config_offset);
		config &= ~0b11 << (irq % 16 * 2);
		config |= (edge ? 0b10 : 0b00) << (irq % 16 * 2);
		DIST_SPACE.store(GICD_ICFGR + config_offset, config);

		// configure affinity
		u64 mpidr;
		asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
		u32 affinity = (mpidr & 0xFFFFFF) | (mpidr >> 32 & 0xFF) << 24;

		u32 router_offset = (irq - 32) * 8;
		u64 router = (affinity & 0xFFFFFF) | static_cast<u64>(affinity >> 24) << 32;
		DIST_SPACE.store(GICD_IROUTER + router_offset, router);

		DIST_SPACE.store(GICD_ISENABLER + enable_offset, 1U << (irq % 32));
	}
}

static bool gic_v3_init(dtb::Node node, dtb::Node parent) {
	return true;

	auto [dist_phys, dist_size] = node.reg(parent.size_cells(), 0).value();
	auto [redist_phys, redist_size] = node.reg(parent.size_cells(), 1).value();

	DIST_SPACE.set_phys(dist_phys, dist_size);
	REDIST_SPACE.set_phys(redist_phys, redist_size);
	assert(DIST_SPACE.map(CacheMode::Uncached));
	assert(REDIST_SPACE.map(CacheMode::Uncached));

	println("GICv3 init");

	DIST_SPACE.store(GICD_CTLR, 0b110000);
	while (DIST_SPACE.load(GICD_CTLR) & 1U << 31);
	DIST_SPACE.store(GICD_CTLR, 0b110111);
	println("ARE enabled");

	// enable redistributor on cpu
	auto waker = REDIST_SPACE.load(GICR_WAKER);
	waker &= ~(1U << 1);
	REDIST_SPACE.store(GICR_WAKER, waker);
	// wait for it to wake up
	while (REDIST_SPACE.load(GICR_WAKER) & 1U << 2);
	println("REDIST enabled");

	// enable cpu interface
	u64 sre;
	asm volatile("mrs %0, icc_sre_el1" : "=r"(sre));
	sre |= 1;
	asm volatile("msr icc_sre_el1, %0" : : "r"(sre));
	println("SRE enabled");

	// configure the priority mask level
	u64 pmr = 0xFF;
	asm volatile("msr icc_pmr_el1, %0" : : "r"(pmr));
	println("PMR configured");

	// configure pre-emption to disabled
	u64 bpr = 0b111;
	asm volatile("msr icc_bpr1_el1, %0" : : "r"(bpr));
	println("BPR configured");

	u64 cpu_ctlr;
	asm volatile("mrs %0, icc_ctlr_el1" : "=r"(cpu_ctlr));
	// eoir does both priority drop and deactivation
	cpu_ctlr &= ~(1U << 1);
	asm volatile("msr icc_ctlr_el1, %0" : : "r"(cpu_ctlr));
	println("EOI mode configured");

	// enable group 1 irqs
	u64 igrpen1 = 1;
	asm volatile("msr icc_igrpen1_el1, %0" : : "r"(igrpen1));
	println("GROUP1 irqs enabled");

	config_irq(0, true);

	return true;
}

static DtDriver GIC_V3_DRIVER {
	.init = gic_v3_init,
	.compatible {"arm,gic-v3"}
};

DT_DRIVER(GIC_V3_DRIVER);
