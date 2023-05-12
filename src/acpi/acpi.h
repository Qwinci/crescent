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

typedef enum : u8 {
	ACPI_ADDR_SPACE_SYS_MEM = 0,
	ACPI_ADDR_SPACE_SYS_IO = 1,
	ACPI_ADDR_SPACE_PCI_CONF = 2,
	ACPI_ADDR_SPACE_EMBEDDED_CONTROLLER = 3,
	ACPI_ADDR_SPACE_SYS_MANAGEMENT_BUS = 4,
	ACPI_ADDR_SPACE_SYS_CMOS = 5,
	ACPI_ADDR_SPACE_PCI_BAR = 6,
	ACPI_ADDR_SPACE_IPMI = 7,
	ACPI_ADDR_SPACE_GP_IO = 8,
	ACPI_ADDR_SPACE_GP_SERIAL = 9,
	ACPI_ADDR_SPACE_PLATFORM_COM_CHANNEL = 10
} AcpiAddrSpace;

typedef enum : u8 {
	ACPI_ACCESS_SIZE_1 = 1,
	ACPI_ACCESS_SIZE_2 = 2,
	ACPI_ACCESS_SIZE_4 = 3,
	ACPI_ACCESS_SIZE_8 = 4
} AcpiAccessSize;

typedef struct [[gnu::packed]] {
	AcpiAddrSpace addr_space_id;
	u8 bit_width;
	u8 bit_offset;
	AcpiAccessSize access_size;
	u64 address;
} AcpiAddr;

void acpi_init(void* rsdp);
bool acpi_checksum(const void* table, usize size);
const SdtHeader* acpi_get_table(const char* table);
void* acpi_get_rsdp();