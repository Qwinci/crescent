#include "acpi.hpp"
#include "cstring.hpp"
#include "mem/mem.hpp"
#include "stdio.hpp"
#include "assert.hpp"

namespace acpi {
	struct [[gnu::packed]] Rsdt {
		SdtHeader hdr;
		u32 ptr[];
	};
	struct [[gnu::packed]] Xsdt {
		SdtHeader hdr;
		u64 ptr[];
	};

	namespace {
		union {
			Rsdt* rsdt;
			Xsdt* xsdt;
		} ROOT {};
		void* GLOBAL_RSDP = nullptr;
		bool USE_XSDT = false;
	}

	struct [[gnu::packed]] Rsdp {
		char signature[8];
		u8 checksum;
		char oem_id[6];
		u8 revision;
		u32 rsdt_address;
		u32 length;
		u64 xsdt_address;
		u8 extended_checksum;
		u8 reserved[3];
	};

	Fadt* GLOBAL_FADT;

	void init(void* rsdp_ptr) {
		Rsdp* rsdp = static_cast<Rsdp*>(rsdp_ptr);
		GLOBAL_RSDP = rsdp;
		if (rsdp->revision >= 2) {
			ROOT.xsdt = to_virt<Xsdt>(rsdp->xsdt_address);
			USE_XSDT = true;
			println("[acpi]: using XSDT");
		}
		else {
			ROOT.rsdt = to_virt<Rsdt>(rsdp->rsdt_address);
			println("[acpi]: using RSDT");
		}

		GLOBAL_FADT = static_cast<Fadt*>(acpi::get_table("FACP"));
		assert(GLOBAL_FADT);
	}

	void* get_rsdp() {
		return GLOBAL_RSDP;
	}

	void* get_table(const char (&signature)[5], u32 index) {
#if __x86_64__
		if (USE_XSDT) {
			u32 count = (ROOT.xsdt->hdr.length - sizeof(Xsdt)) / 8;
			for (u32 i = 0; i < count; ++i) {
				auto* hdr = to_virt<SdtHeader>(ROOT.xsdt->ptr[i]);
				if (memcmp(hdr->signature, signature, 4) == 0 && index-- == 0) {
					return hdr;
				}
			}
		}
		else {
			u32 count = (ROOT.rsdt->hdr.length - sizeof(Rsdt)) / 4;
			for (u32 i = 0; i < count; ++i) {
				auto* hdr = to_virt<SdtHeader>(ROOT.rsdt->ptr[i]);
				if (memcmp(hdr->signature, signature, 4) == 0 && index-- == 0) {
					return hdr;
				}
			}
		}
#endif

		return nullptr;
	}
}
