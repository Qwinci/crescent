#include "discovery.hpp"
#include "arch/aarch64/kernel_dtb.hpp"
#include "utils/driver.hpp"

extern Driver DRIVERS_START[];
extern Driver DRIVERS_END[];

static void try_find_driver(DtbNode& node) {
	if (node.compatible.is_empty()) {
		return;
	}

	for (Driver* driver = DRIVERS_START; driver != DRIVERS_END; ++driver) {
		if (driver->type == Driver::Type::Dt) {
			for (auto driver_compatible : driver->dt->compatible) {
				if (node.is_compatible(driver_compatible)) {
					driver->dt->init(node);
					goto end;
				}
			}
		}
	}

	end:
}

void dtb_discover_devices() {
	DTB->traverse(
		[](DtbNode&) {return true;},
		[](DtbNode& node) {
			try_find_driver(node);
			return false;
	});
}
