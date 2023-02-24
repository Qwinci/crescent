#include "nic.hpp"

Nic* nics {};
static Nic* nics_end {};
usize nic_count {};

void register_nic(Nic* nic) {
	nic->next = nullptr;
	if (nics) {
		nics_end->next = nic;
		nics_end = nic;
	}
	else {
		nics = nic;
		nics_end = nic;
	}
	++nic_count;
}