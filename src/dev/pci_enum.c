#include "pci_enum.h"
#include "acpi/acpi.h"
#include "dev.h"
#include "mem/utils.h"
#include "pci.h"

extern Driver DRIVERS_START;
extern Driver DRIVERS_END;

typedef struct {
	u64 base;
	u16 seg_group;
	u8 start_bus;
	u8 end_bus;
	u8 reserved[4];
} McfgEntry;

typedef struct [[gnu::packed]] {
	SdtHeader hdr;
	u8 reserved[8];
	McfgEntry entries[];
} Mcfg;
static_assert(sizeof(Mcfg) == sizeof(SdtHeader) + 8);

static void enum_func(void* base, u16 func) {
	base = offset(base, void*, (usize) func << 12);

	PciCommonHdr* common_hdr = (PciCommonHdr*) base;

	if (common_hdr->vendor_id == 0xFFFF) {
		return;
	}

	if (common_hdr->hdr_type != 0) {
		// todo
		//kprintf("unsupported pci device: %x:%x\n", common_hdr->device_id, common_hdr->vendor_id);
		return;
	}

	//kprintf("pci device %02x:%02x\n", common_hdr->device_id, common_hdr->vendor_id);

	PciHdr0* hdr = (PciHdr0*) common_hdr;

	for (Driver* driver = &DRIVERS_START; driver < &DRIVERS_END; ++driver) {
		if (driver->type == DRIVER_PCI) {
			PciDriver* pci_driver = driver->pci_driver;

			if (pci_driver->match & PCI_MATCH_CLASS && common_hdr->class_code != pci_driver->dev_class) {
				continue;
			}
			if (pci_driver->match & PCI_MATCH_SUBCLASS && common_hdr->subclass != pci_driver->dev_subclass) {
				continue;
			}
			if (pci_driver->match & PCI_MATCH_PROG && common_hdr->prog_if != pci_driver->dev_prog) {
				continue;
			}
			if (pci_driver->match & PCI_MATCH_DEV) {
				for (usize i = 0; i < pci_driver->dev_count; ++i) {
					PciDev dev = pci_driver->devices[i];
					if (common_hdr->vendor_id == dev.vendor && common_hdr->device_id == dev.device) {
						pci_driver->load(hdr);
						break;
					}
				}
			}
			else {
				pci_driver->load(hdr);
				break;
			}
		}
	}
}

static void enum_dev(void* base, u16 dev) {
	base = offset(base, void*, (usize) dev << 15);

	PciCommonHdr* hdr = (PciCommonHdr*) base;

	if (hdr->vendor_id == 0xFFFF) {
		return;
	}

	if (hdr->hdr_type & 1 << 7) {
		for (u8 func = 0; func < 8; ++func) {
			enum_func(base, func);
		}
	}
	else {
		enum_func(base, 0);
	}
}

static void enum_bus(void* base, u16 bus) {
	base = offset(base, void*, (usize) bus << 20);

	PciCommonHdr* hdr = (PciCommonHdr*) base;

	if (hdr->vendor_id == 0xFFFF) {
		return;
	}

	for (u16 dev = 0; dev < 32; ++dev) {
		enum_dev(base, dev);
	}
}

static const Mcfg* g_mcfg = NULL;

void* pci_get_space(u16 seg, u16 bus, u16 dev, u16 func) {
	assert(g_mcfg);

	u16 count = (g_mcfg->hdr.length - sizeof(Mcfg)) / sizeof(McfgEntry);

	for (u16 i = 0; i < count; ++i) {
		const McfgEntry* entry = &g_mcfg->entries[i];
		if (entry->seg_group == seg) {
			void* res = to_virt(entry->base);
			res = offset(res, void*, (usize) (bus - entry->start_bus) << 20);
			res = offset(res, void*, (usize) dev << 15);
			res = offset(res, void*, (usize) func << 12);
			return res;
		}
	}
	return NULL;
}

void pci_init() {
	const Mcfg* mcfg = (const Mcfg*) acpi_get_table("MCFG");
	g_mcfg = mcfg;

	u16 count = (mcfg->hdr.length - sizeof(Mcfg)) / sizeof(McfgEntry);

	for (u16 i = 0; i < count; ++i) {
		const McfgEntry* entry = &mcfg->entries[i];

		for (u32 bus = entry->start_bus; bus < entry->end_bus; ++bus) {
			enum_bus(to_virt(entry->base), bus - entry->start_bus);
		}
	}
}