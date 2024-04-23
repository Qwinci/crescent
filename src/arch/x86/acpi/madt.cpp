#include "acpi/acpi.hpp"
#include "arch/x86/dev/io_apic.hpp"
#include "x86/io.hpp"
#include "cstring.hpp"

void x86_madt_parse() {
	auto* madt = static_cast<acpi::Madt*>(acpi::get_table("APIC"));
	if (!madt) {
		return;
	}

	if (madt->flags & acpi::Madt::FLAG_LEGACY_PIC) {
		x86::out1(0x21, 0xFF);
		x86::out1(0xA1, 0xFF);
	}

	u32 off = sizeof(acpi::Madt);
	u32 len = madt->hdr.length;
	auto ptr = reinterpret_cast<const u8*>(madt) + sizeof(acpi::Madt);
	while (off < len) {
		auto entry_type = ptr[0];
		auto entry_len = ptr[1];

		// IO APIC
		if (entry_type == 1) {
			u32 addr;
			u32 gsi_base;
			memcpy(&addr, &ptr[4], 4);
			memcpy(&gsi_base, &ptr[8], 4);

			IO_APIC.register_io_apic(addr, gsi_base);
		}
		// IO APIC irq source override
		else if (entry_type == 2) {
			u8 irq_num = ptr[3];

			u32 gsi;
			memcpy(&gsi, &ptr[4], 4);
			u16 flags;
			memcpy(&flags, &ptr[8], 2);

			u8 polarity = flags & 0b11;
			u8 trigger = flags >> 2 & 0b11;

			IO_APIC.register_override(irq_num, gsi, polarity == 0b11, trigger == 0b11);
		}

		off += entry_len;
		ptr += entry_len;
	}
}
