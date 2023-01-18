#pragma once
#include "types.hpp"

struct SdtHeader {
	char signature[4];
	u32 length;
	u8 revision;
	u8 checksum;
	char oem_id[6];
	char oem_table_id[8];
	u32 oem_revision;
	char creator_id[4];
	u32 creator_revision;
};

static_assert(sizeof(SdtHeader) == 36);

SdtHeader* locate_acpi_table(const void* rsdp, const char* name);

void parse_madt(const void* madt_ptr);