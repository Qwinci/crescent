#include "qcom_geni_i2c.hpp"
#include "mem/iospace.hpp"
#include "utils/driver.hpp"

namespace geni_regs {
	static constexpr BitRegister<u32> IF_DISABLE_RO {0x64};
}

namespace if_disable {
	static constexpr BitField<u32, bool> FIFO_DISABLE {0, 1};
}

static InitStatus geni_init(DtbNode& node) {
	if (auto status = node.prop("status")) {
		if (status.value().as_string() == "disabled") {
			println("ignoring disabled geni i2c");
			return InitStatus::Success;
		}
	}

	auto [base, size] = node.reg_by_index(0).value();
	println("geni base ", Fmt::Hex, base, " size ", size, Fmt::Reset);
	IoSpace space {base, size};
	auto status = space.map(CacheMode::Uncached);
	assert(status);

	println("Loading disable");
	auto disable = space.load(geni_regs::IF_DISABLE_RO);
	println("disable: ", Fmt::Hex, disable, Fmt::Reset);
	if (disable & if_disable::FIFO_DISABLE) {
		println("fifo is disabled");
	}
	else {
		println("fifo is enabled");
	}

	return InitStatus::Success;
}

static DtDriver GENI_I2C_DRIVER {
	.name = "Qualcomm Geni I2C",
	.init = geni_init,
	.compatible {
		"qcom,i2c-geni"
	}
};

DT_DRIVER(GENI_I2C_DRIVER);
