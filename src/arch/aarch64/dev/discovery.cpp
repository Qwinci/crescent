#include "discovery.hpp"
#include "arch/aarch64/kernel_dtb.hpp"
#include "utils/driver.hpp"
#include "manually_destroy.hpp"

extern Driver DRIVERS_START[];
extern Driver DRIVERS_END[];

namespace {
	ManuallyDestroy<kstd::vector<Driver*>> LOADED_DRIVERS;
	ManuallyDestroy<kstd::vector<kstd::pair<Driver*, DtbNode*>>> PENDING_DRIVERS;
}

static void try_find_driver(DtbNode& node) {
	if (node.compatible.is_empty()) {
		return;
	}

	for (Driver* driver = DRIVERS_START; driver != DRIVERS_END; ++driver) {
		if (driver->type == Driver::Type::Dt) {
			for (auto driver_compatible : driver->dt->compatible) {
				if (node.is_compatible(driver_compatible)) {
					bool can_load = true;
					for (auto dependency : driver->dt->depends) {
						bool dep_found = false;
						for (auto loaded : *LOADED_DRIVERS) {
							for (auto provide : loaded->dt->provides) {
								if (provide == dependency) {
									dep_found = true;
									break;
								}
							}

							if (dep_found) {
								break;
							}
						}

						if (!dep_found) {
							can_load = false;
							break;
						}
					}

					if (can_load) {
						if (driver->dt->init(node)) {
							for (auto pending : *PENDING_DRIVERS) {
								if (!pending.first) {
									continue;
								}

								bool can_load_pending = true;
								for (auto dependency : pending.first->dt->depends) {
									bool dep_found = false;

									for (auto provide : driver->dt->provides) {
										if (provide == dependency) {
											dep_found = true;
											break;
										}
									}

									if (!dep_found) {
										for (auto loaded : *LOADED_DRIVERS) {
											for (auto provide : loaded->dt->provides) {
												if (provide == dependency) {
													dep_found = true;
													break;
												}
											}
										}
									}

									if (!dep_found) {
										can_load_pending = false;
										break;
									}
								}

								if (can_load_pending) {
									pending.first->dt->init(*pending.second);
									pending.first = nullptr;
								}
							}

							LOADED_DRIVERS->push(driver);
						}
					}
					else {
						PENDING_DRIVERS->push({driver, &node});
					}

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
