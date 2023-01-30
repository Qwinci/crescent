#pragma once
#include "pci.hpp"

#ifdef WITH_NVME
	void init_nvme(Pci::Header0* hdr);
#endif