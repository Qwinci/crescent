#include "pci.h"
#include "arch/cpu.h"
#include "arch/map.h"
#include "mem/page.h"
#include "mem/utils.h"
#include "utils/math.h"
#include "mem/allocator.h"
#include "arch/interrupts.h"

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

void pci_msi_set(PciMsiCap* self, bool enable, u8 vec, Cpu* cpu);
void pci_msix_set_entry(PciMsiXEntry* self, bool mask, u8 vec, Cpu* cpu);

u32 pci_irq_alloc(PciDev* self, u32 min, u32 max, u32 flags) {
	if (self->irq_count) {
		return 0;
	}
	if (max == 0) {
		max = UINT32_MAX;
	}

	bool use_msi = (flags & PCI_IRQ_ALLOC_MSI) && self->msi;
	bool use_msix = (flags & PCI_IRQ_ALLOC_MSIX) && self->msix;

	u32 count;
	if (use_msix) {
		count = (self->msix->msg_control & 0x7FF) + 1;
	}
	else if (use_msi) {
		count = 1 << (self->msi->msg_control >> 1 & 0b111);
	}
	else {
		panic("[kernel][pci]: legacy irqs are not supported\n");
	}

	if (count < min) {
		return 0;
	}

	count = MIN(count, max);

	self->irqs_shared = flags & PCI_IRQ_ALLOC_SHARED;
	IrqInstallFlags irq_install_flags = self->irqs_shared ? IRQ_INSTALL_SHARED : IRQ_INSTALL_FLAG_NONE;
	if (use_msix) {
		self->irqs = kmalloc(count * sizeof(u32));
		assert(self->irqs);
		self->irq_count = count;

		u8 bar = pci_msix_get_table_bar(self->msix);
		PciMsiXEntry* entries = (PciMsiXEntry*) to_virt(pci_map_bar(self->hdr0, bar) + pci_msix_get_table_offset(self->msix));
		pci_msix_mask_all(self->msix, true);
		pci_msix_enable(self->msix, true);

		for (u32 i = 0; i < count; ++i) {
			u32 irq = arch_irq_alloc_generic(IPL_DEV, 1, irq_install_flags);
			assert(irq);
			self->irqs[i] = irq;
			pci_msix_set_entry(&entries[i], false, irq, arch_get_cpu(0));
		}
	}
	else if (use_msi) {
		u32 irqs = arch_irq_alloc_generic(IPL_DEV, count, irq_install_flags);
		if (!irqs) {
			return 0;
		}

		self->irqs = kmalloc(count * sizeof(u32));
		assert(self->irqs);
		self->irq_count = count;
		pci_msi_set(self->msi, false, irqs, arch_get_cpu(0));

		for (u32 i = 0; i < count; ++i) {
			self->irqs[i] = irqs + i;
		}
	}

	return count;
}

u32 pci_irq_get(PciDev* self, u32 index) {
	if (index < self->irq_count) {
		return self->irqs[index];
	}
	else {
		return 0;
	}
}

void pci_irq_free(PciDev* self) {
	for (u32 i = 0; i < self->irq_count; ++i) {
		arch_irq_dealloc_generic(self->irqs[i], 1, self->irqs_shared ? IRQ_INSTALL_SHARED : IRQ_INSTALL_FLAG_NONE);
	}
	kfree(self->irqs, self->irq_count * sizeof(u32));
	self->irqs = NULL;
	self->irq_count = 0;
}

void pci_dev_destroy(PciDev* self) {
	assert(self->irq_count == 0);
	kfree(self, sizeof(PciDev));
}

void pci_enable_irqs(PciDev* self) {
	if (self->msix) {
		pci_msix_mask_all(self->msix, false);
	}
	else if (self->msi) {
		pci_msi_enable(self->msi, true);
	}
	else {
		panic("[kernel][pci]: legacy interrupts are not supported\n");
	}
}

void pci_disable_irqs(PciDev* self) {
	if (self->msix) {
		pci_msix_mask_all(self->msix, true);
	}
	else if (self->msi) {
		pci_msi_enable(self->msi, false);
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
