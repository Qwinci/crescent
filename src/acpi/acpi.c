#include "acpi.h"
#include "mem/utils.h"
#include "stdio.h"
#include "string.h"

typedef struct [[gnu::packed]] {
	char signature[8];
	u8 checksum;
	char oem_id[6];
	u8 revision;
	u32 rsdt_addr;
	u32 length;
	u64 xsdt_addr;
	u8 extended_checksum;
	u8 reserved[3];
} Rsdp;

bool acpi_checksum(const void* table, usize size) {
	u8 checksum = 0;
	for (usize i = 0; i < size; ++i) {
		checksum += *((u8*) table + i);
	}
	return checksum == 0;
}

typedef struct {
	SdtHeader header;
	u32 ptr[];
} Rsdt;

typedef struct {
	SdtHeader header;
	u64 ptr[];
} Xsdt;

static struct {
	union {
		const Rsdt* rsdt;
		const Xsdt* xsdt;
	};
	bool use_xsdt;
} dt;

void acpi_init(void* rsdp_ptr) {
	const Rsdp* rsdp = (const Rsdp*) rsdp_ptr;
	kprintf("[kernel][acpi]: rsdp revision %u, checksum valid: %d\n", rsdp->revision, acpi_checksum(rsdp, 20));
	if (rsdp->revision == 0) {
		dt.rsdt = (const Rsdt*) to_virt(rsdp->rsdt_addr);
		if (!acpi_checksum(dt.rsdt, dt.rsdt->header.length)) {
			kprintf("[kernel][acpi]: rsdt has invalid checksum");
		}
	}
	else {
		dt.xsdt = (const Xsdt*) to_virt(rsdp->xsdt_addr);
		if (!acpi_checksum(dt.xsdt, dt.xsdt->header.length)) {
			kprintf("[kernel][acpi]: xsdt has invalid checksum");
		}
	}
}

const SdtHeader* acpi_get_table(const char* table) {
	if (!dt.use_xsdt) {
		u32 count = (dt.rsdt->header.length - sizeof(SdtHeader)) / 4;
		for (u32 i = 0; i < count; ++i) {
			const SdtHeader* header = (const SdtHeader*) to_virt(dt.rsdt->ptr[i]);
			if (strncmp(header->signature, table, 4) == 0) {
				return header;
			}
		}
	}
	else {
		u32 count = (dt.rsdt->header.length - sizeof(SdtHeader)) / 8;
		for (u32 i = 0; i < count; ++i) {
			const SdtHeader* header = (const SdtHeader*) to_virt(dt.xsdt->ptr[i]);
			if (strncmp(header->signature, table, 4) == 0) {
				return header;
			}
		}
	}
	return NULL;
}
