#include "utils/driver.hpp"

static InitStatus geni_init(DtbNode& node) {
	return InitStatus::Success;
}

static constexpr DtDriver GENI_DRIVER {
	.name = "Qualcomm SPI GENI driver",
	.init = geni_init,
	.compatible {
		"qcom,spi-geni"
	},
	.depends {
		"gic"
	}
};
