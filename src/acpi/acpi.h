#pragma once
#include "types.h"

typedef struct {
	char signature[4];
	u32 length;
	u8 revision;
	u8 checksum;
	char oem_id[6];
	char oem_table_id[8];
	u32 oem_revision;
	char creator_id[4];
	u32 creator_revision;
} SdtHeader;

typedef struct [[gnu::packed]] {
	u8 addr_space_id;
	u8 reg_bit_width;
	u8 reg_bit_offset;
	u8 reserved;
	u64 address;
} AcpiAddr;

void acpi_init(void* rsdp);
bool acpi_checksum(const void* table, usize size);
const SdtHeader* acpi_get_table(const char* table);