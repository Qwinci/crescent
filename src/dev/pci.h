#pragma once
#include "assert.h"

#define PCI_CMD_INT_DISABLE (1 << 10)
#define PCI_CMD_FAST_BACK_BACK (1 << 9)
#define PCI_CMD_SERR (1 << 8)
#define PCI_CMD_PARITY_ERROR_RESPONSE (1 << 6)
#define PCI_CMD_VGA_PALETTE_SNOOP (1 << 5)
#define PCI_CMD_MEM_WRITE_INVALIDATE (1 << 4)
#define PCI_CMD_SPECIAL_CYCLES (1 << 3)
#define PCI_CMD_BUS_MASTER (1 << 2)
#define PCI_CMD_MEM_SPACE (1 << 1)
#define PCI_CMD_IO_SPACE (1 << 0)

#define PCI_STATUS_PARITY_ERR (1 << 15)
#define PCI_STATUS_SIG_SYSTEM_ERR (1 << 14)
#define PCI_STATUS_REC_MASTER_ABORT (1 << 13)
#define PCI_STATUS_REC_TARGET_ABORT (1 << 12)
#define PCI_STATUS_SIG_TARGET_ABORT (1 << 11)
#define PCI_STATUS_MASTER_DATA_PARITY_ERR (1 << 8)
#define PCI_STATUS_FAST_BACK_BACK_CAPABLE (1 << 7)
#define PCI_STATUS_66MHZ_CAPABLE (1 << 5)
#define PCI_STATUS_CAP_LIST (1 << 4)
#define PCI_STATUS_INT (1 << 3)

typedef struct {
	u16 vendor_id;
	u16 device_id;
	u16 command;
	u16 status;
	u8 rev_id;
	u8 prog_if;
	u8 subclass;
	u8 class_code;
	u8 cache_line_size;
	u8 latency_timer;
	u8 hdr_type;
	u8 bist;
} PciCommonHdr;
static_assert(sizeof(PciCommonHdr) == 16);

typedef struct {
	PciCommonHdr common;
	volatile u32 bars[6]; // 24
	u32 cardbus_cis_ptr; // 4
	u16 subsystem_vendor_id; // 2
	u16 subsystem_id; // 2
	u32 expansion_rom_base; // 4
	u8 cap_ptr; // 1
	u8 reserved[7]; // 7
	u8 int_line; // 1
	u8 int_pin; // 1
	u8 min_grant; // 1
	u8 max_latency; // 1
} PciHdr0;
static_assert(sizeof(PciHdr0) == sizeof(PciCommonHdr) + 48);

typedef struct Cpu Cpu;

typedef enum : u8 {
	PCI_CAP_POWER_MANAGEMENT = 1,
	PCI_CAP_AGP = 2,
	PCI_CAP_VPD = 3,
	PCI_CAP_SLOT_IDENT = 4,
	PCI_CAP_MSI = 5,
	PCI_CAP_HOT_SWAP = 6,
	PCI_CAP_PCI_X = 7,
	PCI_CAP_HYPER_TRANSPORT = 8,
	PCI_CAP_VENDOR = 9,
	PCI_CAP_DEBUG = 10,
	PCI_CAP_CENTRAL_RESOURCE_CONTROL = 11,
	PCI_CAP_HOT_PLUG = 12,
	PCI_CAP_BRIDGE_SUBSYSTEM_VENDOR_ID = 13,
	PCI_CAP_AGP_8X = 14,
	PCI_CAP_SECURE_DEVICE = 15,
	PCI_CAP_PCI_EXPRESS = 16,
	PCI_CAP_MSIX = 17
} PciCap;

typedef struct {
	u32 msg_addr_low;
	u32 msg_addr_high;
	u32 msg_data;
	u32 vector_ctrl;
} PciMsiXEntry;

typedef struct {
	u8 id;
	u8 next;
	u16 msg_control;
	u32 table_off_bir;
	u32 pba_off_bir;
} PciMsiXCap;

typedef struct {
	u8 id;
	u8 next;
	u16 msg_control;
	u32 msg_low;
	u32 msg_high;
	u16 msg_data;
	u16 reserved;
	volatile u32 mask;
	volatile u32 pending;
} PciMsiCap;

typedef struct {
	PciHdr0* hdr0;
	PciMsiCap* msi;
	PciMsiXCap* msix;
	u32* irqs;
	usize irq_count;
	bool irqs_shared;
} PciDev;

void pci_enable_irqs(PciDev* self);
void pci_disable_irqs(PciDev* self);

#define PCI_IRQ_ALLOC_MSI (1 << 0)
#define PCI_IRQ_ALLOC_MSIX (1 << 1)
#define PCI_IRQ_ALLOC_LEGACY (1 << 2)
#define PCI_IRQ_ALLOC_ALL (PCI_IRQ_ALLOC_MSI | PCI_IRQ_ALLOC_MSIX | PCI_IRQ_ALLOC_LEGACY)
#define PCI_IRQ_ALLOC_SHARED (1 << 3)

u32 pci_irq_alloc(PciDev* self, u32 min, u32 max, u32 flags);
u32 pci_irq_get(PciDev* self, u32 index);
void pci_irq_free(PciDev* self);
void pci_dev_destroy(PciDev* self);

void* pci_get_cap(PciHdr0* hdr, PciCap cap, usize index);
bool pci_is_io_space(PciHdr0* hdr, u8 bar);
u32 pci_get_io_bar(PciHdr0* hdr, u8 bar);
usize pci_map_bar(PciHdr0* hdr, u8 bar);
usize pci_get_bar_size(PciHdr0* hdr, u8 bar);
