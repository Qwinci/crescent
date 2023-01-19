#include "common.hpp"
#include "console.hpp"
#include "memory/map.hpp"
#include "utils.hpp"
#include "noalloc/string.hpp"

struct RsdpHeader {
	char signature[8];
	u8 checksum;
	char oem_id[6];
	u8 revision;
	u32 rsdt_addr;
	u32 length;
	u64 xsdt_addr;
	u8 extended_checksum;
	u8 reserved[3];
};

struct [[gnu::packed]] Xsdt {
	SdtHeader header;
	u64 ptr[];
};

struct [[gnu::packed]] Rsdt {
	SdtHeader header;
	u32 ptr[];
};

SdtHeader* locate_acpi_table(const void* rsdp, const char* name) {
	auto rsdp_hdr = cast<const RsdpHeader*>(rsdp);
	if (rsdp_hdr->revision == 2) {
		const Xsdt* xsdt = cast<const Xsdt*>(PhysAddr {as<usize>(rsdp_hdr->xsdt_addr)}.to_virt().as_usize());
		u32 count = (xsdt->header.length - sizeof(SdtHeader)) / 8;

		for (u32 i = 0; i < count; ++i) {
			if (!xsdt->ptr[i]) continue;
			auto header = cast<SdtHeader*>(PhysAddr {xsdt->ptr[i]}.to_virt().as_usize());
			if (noalloc::String {header->signature, 4} == name) {
				return header;
			}
		}
	}

	const Rsdt* rsdt = cast<const Rsdt*>(PhysAddr {as<usize>(rsdp_hdr->rsdt_addr)}.to_virt().as_usize());
	u32 count = (rsdt->header.length - sizeof(SdtHeader)) / 4;

	for (u32 i = 0; i < count; ++i) {
		auto header = cast<SdtHeader*>(PhysAddr {as<usize>(rsdt->ptr[i])}.to_virt().as_usize());
		if (noalloc::String {header->signature, 4} == name) {
			return header;
		}
	}

	return nullptr;
}