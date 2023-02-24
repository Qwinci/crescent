#include "pci.hpp"
#include "acpi/common.hpp"
#include "console.hpp"
#include "dev.hpp"
#include "memory/map.hpp"

using namespace Pci;

static void enum_func(VirtAddr base, u8 function) {
	base = VirtAddr {base.as_usize() + (as<usize>(function) << 12)};
	get_map()->map(base, base.to_phys(), PageFlags::Rw | PageFlags::CacheDisable, true);
	get_map()->refresh_page(base.as_usize());

	auto* common_hdr = cast<CommonHeader*>(base.as_usize());

	if (common_hdr->vendor_id == 0xFFFF) {
		return;
	}

	if ((common_hdr->hdr_type & ~(1 << 7)) != 0) {
		// todo
		/*println("unsupported pci device: ",
				Fmt::Hex, common_hdr->vendor_id, ":", common_hdr->device_id,
				", class ", common_hdr->hdr_type & ~(1 << 7), Fmt::Dec);*/
		return;
	}

	auto* hdr = cast<Header0*>(common_hdr);

	// todo
	//println("pci device: ", Fmt::Hex, hdr->common.vendor_id, ":", hdr->common.device_id, Fmt::Dec);

	for (auto driver = DRIVERS_START; driver < DRIVERS_END; ++driver) {
		if (driver->type == Driver::PCI) {
			auto pci_dev = driver->pci_dev;
			if (pci_dev->match & PciDriver::MATCH_CLASS) {
				if (common_hdr->class_code != pci_dev->dev_class) {
					continue;
				}
			}
			if (pci_dev->match & PciDriver::MATCH_SUBCLASS) {
				if (common_hdr->subclass != pci_dev->dev_subclass) {
					continue;
				}
			}
			if (pci_dev->match & PciDriver::MATCH_PROG) {
				if (common_hdr->prog_if != pci_dev->dev_prog) {
					continue;
				}
			}
			if (pci_dev->match & PciDriver::MATCH_DEV) {
				for (usize i = 0; i < pci_dev->dev_count; ++i) {
					auto dev = pci_dev->devices[i];
					if (common_hdr->vendor_id == dev.vendor && common_hdr->device_id == dev.device) {
						pci_dev->load(hdr);
						break;
					}
				}
			}
			else {
				pci_dev->load(hdr);
			}
		}
	}

#ifdef WITH_NVME
	// Mass Storage Controller
	if (hdr->common.class_code == 0x1
		// Non-Volatile Memory Controller
		&& hdr->common.subclass == 0x8 &&
		// NVM Express
		hdr->common.prog_if == 0x2
		) {
		//init_nvme(hdr);
	}
#endif
}

static void enum_dev(VirtAddr base, u8 device) {
	base = VirtAddr {base.as_usize() + (as<usize>(device) << 15)};

	const auto* hdr = cast<const CommonHeader*>(base.as_usize());

	if (hdr->vendor_id == 0xFFFF) {
		return;
	}

	if (hdr->hdr_type & 1 << 7) {
		enum_func(base, 0);
	}
	else {
		for (u8 function = 0; function < 8; ++function) {
			enum_func(base, function);
		}
	}
}

static void enum_bus(VirtAddr base, u8 bus) {
	base = VirtAddr {base.as_usize() + (as<usize>(bus) << 20)};

	const auto* hdr = cast<const CommonHeader*>(base.as_usize());

	if (hdr->vendor_id == 0xFFFF) {
		return;
	}

	for (u8 device = 0; device < 32; ++device) {
		enum_dev(base, device);
	}
}

void init_pci(void* rsdp) {
	void* mcfg_ptr = locate_acpi_table(rsdp, "MCFG");
	if (!mcfg_ptr) {
		panic("no mcfg table found");
	}

	const Mcfg* mcfg = cast<const Mcfg*>(mcfg_ptr);
	u32 entries = (mcfg->header.length - sizeof(Mcfg)) / sizeof(ConfEntry);

	for (u32 seg_i = 0; seg_i < entries; ++seg_i) {
		auto& seg = mcfg->entries[seg_i];
		for (u8 bus_i = seg.start; bus_i < seg.end; ++bus_i) {
			enum_bus(PhysAddr {seg.base}.to_virt(), bus_i);
		}
	}
}

void* Header0::get_cap(Cap cap) {
	u8* ptr = cast<u8*>(this) + (cap_ptr & ~(0b11));
	while (true) {
		if (*ptr == as<u8>(cap)) {
			return ptr;
		}
		if (!ptr[1]) {
			break;
		}
		ptr = cast<u8*>(this) + ptr[1];
	}
	return nullptr;
}

usize Header0::get_bar_size(u8 bar) {
	auto saved = bars[bar];
	bars[bar] = 0xFFFFFFFF;
	auto size = bars[bar];
	bars[bar] = saved;
	size &= ~0b1111;
	size = ~size;
	return size + 1;
}

usize Header0::map_bar(u8 bar) {
	usize addr;
	auto first = bars[bar];
	if ((first >> 1 & 0b11) == 2) {
		auto second = bars[bar + 1];
		addr = as<usize>(first & 0xFFFFFFF0) + (as<usize>(second) << 32);
	}
	else {
		addr = first & 0xFFFFFFF0;
	}

	auto size = get_bar_size(bar);

	size += addr & (PAGE_SIZE - 1);
	addr = ALIGNDOWN(addr, PAGE_SIZE);

	PageFlags flags = PageFlags::Rw | PageFlags::Nx | (first & 1 << 3 ? PageFlags::WriteThrough : PageFlags::CacheDisable);
	usize i = 0;
	auto phys = PhysAddr {addr};

	while ((addr + i) & (SIZE_2MB - 1) && i < size) {
		get_map()->map(phys.to_virt().offset(i), phys.offset(i), flags, true);
		get_map()->refresh_page(phys.to_virt().offset(i).as_usize());
		i += 0x1000;
	}

	while (i + SIZE_2MB <= size) {
		get_map()->map(phys.to_virt().offset(i), phys.offset(i), flags | PageFlags::Huge, true);
		get_map()->refresh_page(phys.to_virt().offset(i).as_usize());
		i += SIZE_2MB;
	}

	while (i < size) {
		get_map()->map(phys.to_virt().offset(i), phys.offset(i), flags, true);
		get_map()->refresh_page(phys.to_virt().offset(i).as_usize());
		i += 0x1000;
	}

	return addr;
}
