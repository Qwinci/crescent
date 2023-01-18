#include "common.hpp"
#include "console.hpp"
#include "io.hpp"
#include "utils.hpp"

struct Madt {
	SdtHeader header;
	u32 lapic_addr;
	u32 flags;
};

void parse_madt(const void* madt_ptr) {
	auto madt = cast<const Madt*>(madt_ptr);

	if (madt->flags & 1) {
		out1(0xA1, 0xFF);
		out1(0x21, 0xFF);
	}

	usize lapic_phys = madt->lapic_addr;

	for (usize i = sizeof(Madt); i < madt->header.length;) {
		u8 type = *cast<const u8*>(offset(madt, as<isize>(i)));
		i += 1;
		u8 length = *cast<const u8*>(offset(madt, as<isize>(i)));
		i += 1;

		// Processor Local APIC
		if (type == 0) {
			u8 acpi_cpu_id = *cast<const u8*>(offset(madt, as<isize>(i)));
			i += 1;
			u8 apic_id = *cast<const u8*>(offset(madt, as<isize>(i)));
			i += 1;
			u32 flags = *cast<const u32*>(offset(madt, as<isize>(i)));
			i += 4;
		}
		// IO APIC
		else if (type == 1) {
			u8 io_apic_id = *cast<const u8*>(offset(madt, as<isize>(i)));
			i += 1;
			// reserved
			i += 1;
			u32 io_apic_addr = *cast<const u32*>(offset(madt, as<isize>(i)));
			i += 4;
			u32 global_int_base = *cast<const u32*>(offset(madt, as<isize>(i)));
			i += 4;
		}
		// IO APIC Interrupt Source Override
		else if (type == 2) {
			u8 bus_src = *cast<const u8*>(offset(madt, as<isize>(i)));
			i += 1;
			u8 irq_src = *cast<const u8*>(offset(madt, as<isize>(i)));
			i += 1;
			u32 global_int = *cast<const u32*>(offset(madt, as<isize>(i)));
			i += 4;
			u16 flags = *cast<const u16*>(offset(madt, as<isize>(i)));
			i += 2;
		}
		// IO APIC NMI Interrupt Source
		else if (type == 3) {
			u8 nmi_src = *cast<const u8*>(offset(madt, as<isize>(i)));
			i += 1;
			// reserved
			i += 1;
			u16 flags = *cast<const u16*>(offset(madt, as<isize>(i)));
			i += 2;
			u32 global_int = *cast<const u32*>(offset(madt, as<isize>(i)));
			i += 4;
		}
		// Local APIC NMI
		else if (type == 4) {
			u8 acpi_cpu_id = *cast<const u8*>(offset(madt, as<isize>(i)));
			i += 1;
			u16 flags = *cast<const u16*>(offset(madt, as<isize>(i)));
			i += 2;
			u8 lint = *cast<const u8*>(offset(madt, as<isize>(i)));
			i += 1;
		}
		// Local APIC Address Override
		else if (type == 5) {
			// reserved
			i += 2;
			u64 lapic_addr = *cast<const u64*>(offset(madt, as<isize>(i)));
			i += 8;
			lapic_phys = lapic_addr;
		}
		// Processor Local x2APIC
		else if (type == 9) {
			// reserved
			i += 2;
			u32 x2apic_id = *cast<const u32*>(offset(madt, as<isize>(i)));
			i += 4;
			u32 flags = *cast<const u32*>(offset(madt, as<isize>(i)));
			i += 4;
			u32 acpi_id = *cast<const u32*>(offset(madt, as<isize>(i)));
			i += 4;
		}
	}
}