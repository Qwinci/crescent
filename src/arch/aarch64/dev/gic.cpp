#include "gic.hpp"
#include "gic_v3.hpp"

void gic_init_on_cpu() {
	if (GIC_VERSION == 3) {
		gic_v3_init_on_cpu();
	}
}

Gic* GIC = nullptr;
u32 GIC_VERSION = 0;
