#pragma once
#include "pci.hpp"

#ifdef WITH_NVME
	void init_nvme(Pci::Header0* hdr);
#endif

#ifdef WITH_INTEL_GRAPHICS
	void init_intel_graphics(Pci::Header0* hdr);
#endif

#ifdef WITH_RTL8169
	void init_rtl8169(Pci::Header0* hdr);
#endif