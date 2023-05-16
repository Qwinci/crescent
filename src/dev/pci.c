#include "pci.h"
#include "arch/cpu.h"
#include "arch/map.h"
#include "mem/page.h"
#include "mem/utils.h"
#include "timer.h"

static inline u8 pci_msix_get_table_bar(PciMsiXCap* self) {
	return self->table_off_bir & 0b111;
}

static inline u32 pci_msix_get_table_offset(PciMsiXCap* self) {
	return self->table_off_bir & ~0b111;
}

static inline void pci_msix_enable(PciMsiXCap* self, bool enable) {
	if (enable) {
		self->msg_control |= 1 << 15;
	}
	else {
		self->msg_control &= ~(1 << 15);
	}
}

static inline void pci_msix_mask_all(PciMsiXCap* self, bool mask) {
	if (mask) {
		self->msg_control |= 1 << 14;
	}
	else {
		self->msg_control &= ~(1 << 14);
	}
}

static inline void pci_msi_enable(PciMsiCap* self, bool enable) {
	if (enable) {
		self->msg_control |= 1;
	}
	else {
		self->msg_control &= ~1;
	}
}

void pci_set_irq(PciHdr0* hdr, u8 num, u8 vec, Cpu* cpu) {
	PciMsiXCap* msix;
	PciMsiCap* msi;
	if ((msix = pci_get_cap(hdr, PCI_CAP_MSIX, 0))) {
		u8 bar = pci_msix_get_table_bar(msix);
		PciMsiXEntry* entries = (PciMsiXEntry*) to_virt(pci_map_bar(hdr, bar) + pci_msix_get_table_offset(msix));
		pci_msix_set_entry(&entries[num], false, vec, arch_get_cpu(0));
		pci_msix_mask_all(msix, true);
		pci_msix_enable(msix, true);
	}
	else if ((msi = pci_get_cap(hdr, PCI_CAP_MSI, 0))) {
		pci_msi_set(msi, false, vec, arch_get_cpu(0));
	}
	else {
		panic("[kernel][pci]: legacy interrupts are not supported\n");
	}
}

void pci_enable_irqs(PciHdr0* hdr) {
	PciMsiXCap* msix;
	PciMsiCap* msi;
	if ((msix = pci_get_cap(hdr, PCI_CAP_MSIX, 0))) {
		pci_msix_mask_all(msix, false);
	}
	else if ((msi = pci_get_cap(hdr, PCI_CAP_MSI, 0))) {
		pci_msi_enable(msi, true);
	}
	else {
		panic("[kernel][pci]: legacy interrupts are not supported\n");
	}
}

void pci_disable_irqs(PciHdr0* hdr) {
	PciMsiXCap* msix;
	PciMsiCap* msi;
	if ((msix = pci_get_cap(hdr, PCI_CAP_MSIX, 0))) {
		pci_msix_mask_all(msix, true);
	}
	else if ((msi = pci_get_cap(hdr, PCI_CAP_MSI, 0))) {
		pci_msi_enable(msi, false);
	}
	else {
		panic("[kernel][pci]: legacy interrupts are not supported\n");
	}
}

void* pci_get_cap(PciHdr0* hdr, PciCap cap, usize index) {
	u8* ptr = (u8*) hdr + (hdr->cap_ptr & ~0b11);
	while (true) {
		if (*ptr == (u8) cap) {
			if (index-- == 0) {
				return ptr;
			}
		}
		if (!ptr[1]) {
			break;
		}
		ptr = (u8*) hdr + ptr[1];
	}
	return NULL;
}

bool pci_is_io_space(PciHdr0* hdr, u8 bar) {
	return hdr->bars[bar] & 1;
}

u32 pci_get_io_bar(PciHdr0* hdr, u8 bar) {
	return hdr->bars[bar] & ~0b11;
}

usize pci_get_bar_size(PciHdr0* hdr, u8 bar) {
	u32 saved = hdr->bars[bar];
	hdr->bars[bar] = 0xFFFFFFFF;
	u32 size = hdr->bars[bar];
	hdr->bars[bar] = saved;
	size &= ~0b1111;
	return ~size + 1;
}

usize pci_map_bar(PciHdr0* hdr, u8 bar) {
	if (pci_is_io_space(hdr, bar)) {
		return 0;
	}

	u32 first = hdr->bars[bar];
	usize addr = first & 0xFFFFFFF0;
	if ((first >> 1 & 0b11) == 2) {
		u32 second = hdr->bars[bar + 1];
		addr |= (usize) second << 32;
	}

	usize size = pci_get_bar_size(hdr, bar);
	size = ALIGNUP(size, PAGE_SIZE);

	PageFlags flags = PF_READ | PF_WRITE | (first & 1 << 3 ? PF_WT : PF_NC);
	usize i = 0;

	while (i < size) {
		arch_map_page(KERNEL_MAP, (usize) to_virt(addr + i), addr + i, flags | PF_SPLIT);
		i += PAGE_SIZE;
	}

	return addr;
}
