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
	u32 bars[6]; // 24
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