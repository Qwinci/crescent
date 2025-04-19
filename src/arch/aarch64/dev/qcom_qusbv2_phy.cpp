#include "utils/driver.hpp"
#include "dev/reset.hpp"
#include "dev/clock.hpp"
#include "vector.hpp"
#include "mem/iospace.hpp"

struct QusbV2Phy {
	void init() {
		reset();

		for (auto [reg, value] : host_init_seq) {
			space.store(reg, value);
		}
	}

	void reset() {
		phy_reset_controller->set_assert(phy_reset_line, true);
		udelay(100);
		phy_reset_controller->set_assert(phy_reset_line, false);
	}

	IoSpace space;
	ResetController* phy_reset_controller;
	u32 phy_reset_line;
	kstd::vector<kstd::pair<u32, u32>> host_init_seq;
};

static InitStatus qcom_qusbv2_phy_init(DtbNode& node) {
	println("[kernel][aarch64]: qcom qusb2 phy init");

	auto phy_reset_prop = node.prop("resets");
	if (!phy_reset_prop || phy_reset_prop->size != 8) {
		println("[kernel][aarch64]: error: node has missing or invalid phy-reset property");
		return InitStatus::Error;
	}
	u32 phy_reset_handle = phy_reset_prop->read<u32>(0);
	u32 phy_reset_line = phy_reset_prop->read<u32>(4);
	auto phy_reset = *DTB->handle_map.get(phy_reset_handle);
	if (!phy_reset->data) {
		return InitStatus::Defer;
	}

	assert(!node.regs.is_empty());
	IoSpace space {node.regs[0].addr, node.regs[0].size};
	auto status = space.map(CacheMode::Uncached);
	assert(status);

	auto phy = new QusbV2Phy {};

	phy->space = space;
	phy->phy_reset_controller = static_cast<ResetController*>(phy_reset->data);
	phy->phy_reset_line = phy_reset_line;

	auto host_init_seq_prop = node.prop("qcom,qusb-phy-host-init-seq");
	if (host_init_seq_prop) {
		for (u32 i = 0; i < host_init_seq_prop->size; i += 8) {
			u32 value = host_init_seq_prop->read<u32>(i);
			u32 reg = host_init_seq_prop->read<u32>(i + 4);

			phy->host_init_seq.push({reg, value});
		}
	}

	node.data = phy;

	phy->init();

	return InitStatus::Success;
}

static constexpr DtDriver QUSBV2_DRIVER {
	.name = "Qualcomm QUSB2 phy driver",
	.init = qcom_qusbv2_phy_init,
	.compatible {
		"qcom,qusb2phy-v2"
	}
};

DT_DRIVER(QUSBV2_DRIVER);
