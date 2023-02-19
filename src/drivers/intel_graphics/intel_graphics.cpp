#include "drivers/pci_drivers.hpp"

void init_intel_graphics(Pci::Header0* hdr) {
	println("initializing intel graphics");
	auto* power_cap = as<Pci::PowerCap*>(hdr->get_cap(Pci::Cap::PowerManagement));
	if (power_cap) {
		println("power cap found");
		power_cap->set_power(Pci::PowerCap::State::D3);
		power_cap->set_power(Pci::PowerCap::State::D0);
		println("power restored");
	}
}