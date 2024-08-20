#include <vector.hpp>
#include "arch/aarch64/dtb.hpp"
#include "mem/iospace.hpp"
#include "utils/driver.hpp"

constexpr BasicRegister<u32> VERSION {0};
constexpr BitRegister<u32> FEATURES {4};
constexpr BasicRegister<u32> FEATURES2 {8};
constexpr BasicRegister<u32> CHN_CMD {0};
constexpr BasicRegister<u32> CHN_CONFIG {4};
constexpr BitRegister<u32> CHN_STATUS {8};
constexpr BasicRegister<u32> CHN_WDATA0 {0x10};
constexpr BasicRegister<u32> CHN_WDATA1 {0x14};
constexpr BasicRegister<u32> CHN_RDATA0 {0x18};
constexpr BasicRegister<u32> CHN_RDATA1 {0x1C};

constexpr BitField<u32, u16> FEATURES_PERIPH {0, 11};

constexpr BitField<u32, bool> STATUS_DONE {0, 1};
constexpr BitField<u32, bool> STATUS_DENIED {1, 1};
constexpr BitField<u32, bool> STATUS_FAILURE {2, 1};
constexpr BitField<u32, bool> STATUS_DROPPED {3, 1};

struct PmicArb {
	enum class Op {
		ExtWriteL = 0,
		ExtReadL = 1,
		ExtWrite = 2,
		Reset = 3,
		Sleep = 4,
		Shutdown = 5,
		Wakeup = 6,
		Authenticate = 7,
		MasterRead = 8,
		MasterWrite = 9,
		ExtRead = 13,
		Write = 14,
		Read = 15,
		ZeroWrite = 16
	};

	struct Apid {
		u16 ppid;
		u8 write_ee;
		u8 irq_ee;
	};

	u16 ppid_to_apid(u16 ppid) {
		return ppid_apid_table[ppid] & ~(1U << 15);
	}

	enum class Type {
		Read,
		Write
	};

	u32 get_offset(u8 sid, u16 addr, Type type) {
		u16 ppid = sid << 8 | addr >> 8;
		auto apid = ppid_to_apid(ppid);
		u32 offset;
		switch (type) {
			case Type::Read:
				offset = 0x10000 * /* ee */ 0 + 0x80 * apid;
				break;
			case Type::Write:
				offset = 0x10000 * apid;
				break;
		}
		return offset;
	}

	bool read(u8 sid, u16 addr, u8* buffer, u8 len, Op op) {
		assert(len <= 8);

		auto offset = get_offset(sid, addr, Type::Read);

		u8 block_count = len - 1;

		u32 cmd = static_cast<u32>(op) << 27 | (addr & 0xFF) << 4 | block_count;

		auto space = read_space.subspace(offset);
		space.store(CHN_CMD, cmd);

		while (true) {
			auto status = space.load(CHN_STATUS);
			if (status & STATUS_DONE) {
				if (status & STATUS_DENIED) {
					println("pmic arb transaction denied ", Fmt::Hex,
							"sid: ", sid, " addr: ", addr, Fmt::Dec);
					return false;
				}
				else if (status & STATUS_FAILURE) {
					println("pmic arb transaction failed ", Fmt::Hex,
					        "sid: ", sid, " addr: ", addr, Fmt::Dec);
					return false;
				}
				else if (status & STATUS_DROPPED) {
					println("pmic arb transaction dropped ", Fmt::Hex,
					        "sid: ", sid, " addr: ", addr, Fmt::Dec);
					return false;
				}

				break;
			}
		}

		auto data = space.load(CHN_RDATA0);
		memcpy(buffer, &data, kstd::min(len, u8 {4}));
		if (len > 4) {
			data = space.load(CHN_RDATA1);
			memcpy(buffer + 4, &data, len - 4);
		}

		return true;
	}

	bool write(u8 sid, u16 addr, u8* buffer, u8 len, Op op) {
		assert(len <= 8);

		auto offset = get_offset(sid, addr, Type::Write);

		u8 block_count = len - 1;

		u32 cmd = static_cast<u32>(op) << 27 | (addr & 0xFF) << 4 | block_count;

		auto space = write_space.subspace(offset);

		u32 data;
		memcpy(&data, buffer, kstd::min(len, u8 {4}));
		space.store(CHN_WDATA0, data);
		if (len > 4) {
			memcpy(&data, buffer + 4, len - 4);
			space.store(CHN_WDATA1, data);
		}

		space.store(CHN_CMD, cmd);

		while (true) {
			auto status = space.load(CHN_STATUS);
			if (status & STATUS_DONE) {
				if (status & STATUS_DENIED) {
					println("pmic arb transaction denied ", Fmt::Hex,
					        "sid: ", sid, " addr: ", addr, Fmt::Dec);
					return false;
				}
				else if (status & STATUS_FAILURE) {
					println("pmic arb transaction failed ", Fmt::Hex,
					        "sid: ", sid, " addr: ", addr, Fmt::Dec);
					return false;
				}
				else if (status & STATUS_DROPPED) {
					println("pmic arb transaction dropped ", Fmt::Hex,
					        "sid: ", sid, " addr: ", addr, Fmt::Dec);
					return false;
				}

				return true;
			}
		}
	}

	IoSpace core_space;
	IoSpace read_space;
	IoSpace write_space;
	IoSpace cnfg_space;

	kstd::vector<u16> ppid_apid_table;
	kstd::vector<Apid> apids;
	u16 base_apid;
	u16 apid_count;
};

bool spmi_init(DtbNode& node) {
	println("spmi init");

	auto cells = node.parent->cells;

	auto [core_phys, core_size] = node.reg(cells, 0).value();
	auto [chnls_phys, chnls_size] = node.reg(cells, 1).value();
	auto [obsrvr_phys, obsrvr_size] = node.reg(cells, 2).value();
	auto [intr_phys, intr_size] = node.reg(cells, 3).value();
	auto [cnfg_phys, cnfg_size] = node.reg(cells, 4).value();

	IoSpace core_space {core_phys, core_size};
	IoSpace read_space {obsrvr_phys, obsrvr_size};
	IoSpace write_space {chnls_phys, chnls_size};
	IoSpace cnfg_space {cnfg_phys, cnfg_size};
	assert(core_space.map(CacheMode::Uncached));
	assert(read_space.map(CacheMode::Uncached));
	assert(write_space.map(CacheMode::Uncached));
	assert(cnfg_space.map(CacheMode::Uncached));

	auto version = core_space.load(VERSION);
	println("version: ", Fmt::Hex, version);

	PmicArb arb {};
	arb.core_space = core_space;
	arb.read_space = read_space;
	arb.write_space = write_space;
	// 12 bits max
	arb.ppid_apid_table.resize(0xFFF);
	arb.base_apid = 0;
	arb.apid_count = core_space.load(FEATURES) & FEATURES_PERIPH;
	// 512 == max periphs v5
	arb.apids.resize(512);

	for (usize i = arb.base_apid; i < arb.base_apid + arb.apid_count; ++i) {
		auto& apid = arb.apids[i];

		// v5
		u32 offset = 0x900 + 4 * i;

		BasicRegister<u32> reg {offset};
		auto value = core_space.load(reg);
		if (!value) {
			continue;
		}
		u16 ppid = (value >> 8) & 0xFFF;

		bool is_irq_owner = value & 1U << 24;

		// v2 used by v5
		u32 apid_owner_off = 0x700 + 4 * i;
		reg = BasicRegister<u32> {apid_owner_off};
		value = cnfg_space.load(reg);

		apid.write_ee = value & 0x7;
		apid.irq_ee = is_irq_owner ? apid.write_ee : 0xFF;

		bool valid = arb.ppid_apid_table[ppid] & 1U << 15;
		u16 apid_id = arb.ppid_apid_table[ppid] & ~(1U << 15);
		// 00 == arb->ee
		if (!value || apid.write_ee == 0) {
			arb.ppid_apid_table[ppid] = i | 1U << 15;
		}

		apid.ppid = ppid;
	}

	constexpr u32 MPP_REG_RT_STS = 0x10;
	constexpr u32 MPP_REG_RT_STS_VAL_MASK = 0x1;
	constexpr u32 PMIC_GPIO_ADDRESS_RANGE = 0x100;

	u16 addr = 0xC000;
	u8 pin = 1;

	// 0x1 == IRQ_TYPE_EDGE_RISING
	// 0x2 == IRQ_TYPE_EDGE_FALLING
	// 0x3 == IRQ_TYPE_EDGE_BOTH
	// 0x4 == IRQ_TYPE_LEVEL_HIGH
	// 0x8 == IRQ_TYPE_LEVEL_LOW

	u32 base = addr + pin * PMIC_GPIO_ADDRESS_RANGE;
	while (true) {
		u32 value;
		// pmic usid == 0
		arb.read(0, base + MPP_REG_RT_STS, reinterpret_cast<u8*>(&value), 4, PmicArb::Op::Read);

		if (!(value & MPP_REG_RT_STS_VAL_MASK)) {
			println("works");

			u32 pon_base = 0x800;

			auto pon_masked_write = [&](u16 addr, u8 mask, u8 value) {
				u8 reg;
				arb.read(0, addr, &reg, 1, PmicArb::Op::ExtReadL);
				reg &= ~mask;
				reg |= value & mask;
				arb.write(0, addr, &reg, 1, PmicArb::Op::ExtWriteL);
			};

			auto pon_set_reset_reason = [&](u8 reason) {
				constexpr u32 QPNP_PON_SOFT_RB_SPARE = 0x8F;

				u8 mask = 0b11111110;
				pon_masked_write(pon_base + QPNP_PON_SOFT_RB_SPARE, mask, reason << 1);
			};

			constexpr u8 PON_RESTART_REASON_UNKNOWN = 0;
			constexpr u8 PON_RESTART_REASON_RECOVERY = 1;
			constexpr u8 PON_RESTART_REASON_BOOTLOADER = 0x2;
			constexpr u8 PON_RESTART_REASON_DMVERITY_ENFORCE = 0x5;

			pon_set_reset_reason(PON_RESTART_REASON_BOOTLOADER);

			// reset msm but not pmic
			constexpr u8 PON_POWER_OFF_WARM_RESET = 1;
			constexpr u8 PON_POWER_OFF_SHUTDOWN = 4;
			constexpr u8 PON_POWER_OFF_HARD_RESET = 0x7;

			auto pon_system_power_off = [&](u8 type) {
				constexpr u32 QPNP_PON_REVISION2 = 0x1;

				u8 reg;
				arb.read(0, pon_base + QPNP_PON_REVISION2, &reg, 1, PmicArb::Op::ExtReadL);

				constexpr u8 PON_REV2_VALUE = 0;
				constexpr u8 PON_PS_HOLD_RST_CTL = 0x5A;
				constexpr u8 PON_PS_HOLD_RST_CTL2 = 0x5B;

				u8 reset_enable_reg;
				if (reg == PON_REV2_VALUE) {
					reset_enable_reg = PON_PS_HOLD_RST_CTL;
				}
				else {
					reset_enable_reg = PON_PS_HOLD_RST_CTL2;
				}

				constexpr u8 QPNP_PON_RESET_EN = 1U << 7;

				pon_masked_write(pon_base + reset_enable_reg, QPNP_PON_RESET_EN, 0);

				for (usize i = 0; i < 1'000'000'000 / 4; ++i) {
					asm volatile("nop");
				}

				constexpr u8 QPNP_PON_POWER_OFF_MASK = 0xF;

				pon_masked_write(pon_base + PON_PS_HOLD_RST_CTL, QPNP_PON_POWER_OFF_MASK, type);

				pon_masked_write(pon_base + reset_enable_reg, QPNP_PON_RESET_EN, QPNP_PON_RESET_EN);

				IoSpace dump_space {0xC264000, 0x1000};
				assert(dump_space.map(CacheMode::Uncached));

				volatile u32* pshold = (volatile u32*) dump_space.mapping();
				*pshold = 0;
			};

			pon_system_power_off(PON_POWER_OFF_HARD_RESET);

			break;
		}
	}

	return true;
}

static DtDriver SPMI_DRIVER {
	.init = spmi_init,
	.compatible = {"qcom,spmi-pmic-arb"}
};

//DT_DRIVER(SPMI_DRIVER);
