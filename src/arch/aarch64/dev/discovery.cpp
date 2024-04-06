#include "discovery.hpp"
#include "utils/driver.hpp"

extern Driver DRIVERS_START[];
extern Driver DRIVERS_END[];

static void try_find_driver(dtb::Node node, dtb::Node parent) {
	auto compatible_opt = node.compatible();
	if (!compatible_opt) {
		return;
	}
	auto whole_compatible = compatible_opt.value();

	for (Driver* driver = DRIVERS_START; driver != DRIVERS_END; ++driver) {
		if (driver->type == Driver::Type::Dt) {
			size_t pos = 0;
			while (pos < whole_compatible.size()) {
				auto compatible_end = whole_compatible.find(0, pos);
				kstd::string_view compatible;
				if (compatible_end == kstd::string_view::npos) {
					compatible = whole_compatible.substr(pos);
					pos = whole_compatible.size();
				}
				else {
					compatible = whole_compatible.substr(pos, compatible_end - pos);
					pos = compatible_end + 1;
				}

				for (auto driver_compatible : driver->dt->compatible) {
					if (driver_compatible == compatible) {
						driver->dt->init(node, parent);
						goto end;
					}
				}
			}
		}
	}

	end:
}

void dtb_discover_devices(dtb::Dtb& dtb) {
	auto root = dtb.get_root();
	auto soc_opt = root.find_child("soc");
	if (soc_opt) {
		auto soc = soc_opt.value();
		for (auto child_opt = soc.child(); child_opt; child_opt = child_opt.value().next()) {
			auto child = child_opt.value();
			try_find_driver(child, soc);
		}
	}

	for (auto child_opt = root.child(); child_opt; child_opt = child_opt.value().next()) {
		auto child = child_opt.value();
		try_find_driver(child, root);
	}
}
