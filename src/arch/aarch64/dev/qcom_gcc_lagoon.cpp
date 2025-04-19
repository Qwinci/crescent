#include "qcom_reset.hpp"
#include "utils/driver.hpp"

struct QcomLagoonGcc : public QcomResetController {
	constexpr explicit QcomLagoonGcc(IoSpace space) : QcomResetController {space, REGS} {}

private:
	static constexpr BitRegister<u32> REGS[] {
		// GCC_QUSB2PHY_PRIM_BCR
		BitRegister<u32> {0x1D000},
		// GCC_QUSB2PHY_SEC_BCR
		BitRegister<u32> {0x1E000},
		// GCC_SDCC1_BCR
		BitRegister<u32> {0x4B000},
		// GCC_SDCC2_BCR
		BitRegister<u32> {0x20000},
		// GCC_UFS_PHY_BCR
		BitRegister<u32> {0x3A000},
		// GCC_USB30_PRIM_BCR
		BitRegister<u32> {0x1A000},
		// pad
		BitRegister<u32> {0},
		// pad
		BitRegister<u32> {0},
		// pad
		BitRegister<u32> {0},
		// pad
		BitRegister<u32> {0},
		// GCC_USB3_PHY_PRIM_BCR
		BitRegister<u32> {0x1C000},
		// GCC_USB3_DP_PHY_PRIM_BCR
		BitRegister<u32> {0x1C008},
	};
};

static InitStatus qcom_lagoon_gcc_init(DtbNode& node) {
	println("[kernel][aarch64]: qcom lagoon gcc init");

	assert(!node.regs.is_empty());
	IoSpace space {node.regs[0].addr, node.regs[0].size};
	auto status = space.map(CacheMode::Uncached);
	assert(status);
	node.data = static_cast<QcomResetController*>(new QcomLagoonGcc {space});
	return InitStatus::Success;
}

static constexpr DtDriver LAGOON_GCC_DRIVER {
	.name = "Qualcomm Lagoon GCC driver",
	.init = qcom_lagoon_gcc_init,
	.compatible {
		"qcom,lagoon-gcc"
	}
};

DT_DRIVER(LAGOON_GCC_DRIVER);
