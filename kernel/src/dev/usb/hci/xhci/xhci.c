#include "dev/dev.h"
#include "mem/utils.h"

static bool xhci_fine_matcher(PciDev* dev) {
	PciHdr0* hdr = dev->hdr0;
	return hdr->common.class_code == 0xC && hdr->common.subclass == 0x3 &&
		hdr->common.prog_if >= 0x30 && hdr->common.prog_if <= 0x39;
}

typedef struct {
	u8 cap_length;
	u8 reserved;
	u16 hci_version;
	u32 hcs_params1;
	u32 hcs_params2;
	u32 hcs_params3;
	u32 hcc_params1;
	u32 doorbell_off;
	u32 rt_reg_space_off;
} HciCaps;

#define HCS_PARAMS1_MAX_SLOTS(params) ((params) & 0xFF)
#define HCS_PARAMS1_MAX_INTRS(params) ((params) >> 8 & 0x7FF)
#define HCS_PARAMS1_NUM_PORTS(params) ((params) >> 24)

typedef struct {

} HciOpRegs;

static void xhci_load(PciDev* dev) {
	PciHdr0* hdr = dev->hdr0;

	kprintf("[kernel][xhci]: initializing xhci controller %04x:%04x\n", hdr->common.vendor_id, hdr->common.device_id);

	hdr->common.command = PCI_CMD_IO_SPACE | PCI_CMD_MEM_SPACE | PCI_CMD_BUS_MASTER | PCI_CMD_INT_DISABLE;

	if (pci_is_io_space(hdr, 0)) {
		kprintf("[kernel][xhci]: io space controller is not supported\n");
		return;
	}

	usize base_phys = pci_map_bar(hdr, 0);
	void* base_virt = to_virt(base_phys);

	const HciCaps* caps = (const HciCaps*) base_virt;

	const HciOpRegs* op_regs = offset(base_virt, const HciOpRegs*, caps->cap_length);
	u8 max_slots = HCS_PARAMS1_MAX_SLOTS(caps->hcs_params1);
	u16 max_intrs = HCS_PARAMS1_MAX_INTRS(caps->hcs_params1);
	u8 num_ports = HCS_PARAMS1_NUM_PORTS(caps->hcs_params1);

	kprintf("");
}

static PciDriver XHCI_PCI_DEV = {
	.match = 0,
	.load = xhci_load,
	.fine_matcher = xhci_fine_matcher,
	.dev_count = 0
};

PCI_DRIVER(XHCI, &XHCI_PCI_DEV);