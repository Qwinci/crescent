#include "common.hpp"
#include "console.hpp"
#include "io.hpp"
#include "io_apic.hpp"
#include "memory/map.hpp"
#include "utils.hpp"
#include "lapic.hpp"

struct Madt {
	SdtHeader header;
	u32 lapic_addr;
	u32 flags;
};

void parse_madt(const void* madt_ptr) {
	if (!madt_ptr) {
		panic("parse_madt called with null madt");
	}

	auto madt = cast<const Madt*>(madt_ptr);

	if (madt->flags & 1) {
		out1(0xA1, 0xFF);
		out1(0x21, 0xFF);
	}

	usize lapic_phys = madt->lapic_addr;

	u8 bsp_id;
	asm volatile("mov eax, 1; cpuid; shr ebx, 24" : "=b"(bsp_id));

	for (usize i = sizeof(Madt); i < madt->header.length;) {
		u8 type = *cast<const u8*>(offset(madt, as<isize>(i)));
		i += 1;
		// length
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
			// io_apic_id
			i += 1;
			// reserved
			i += 1;
			u32 io_apic_addr = *cast<const u32*>(offset(madt, as<isize>(i)));
			i += 4;
			u32 global_int_base = *cast<const u32*>(offset(madt, as<isize>(i)));
			i += 4;

			auto& apic = IoApic::apics[IoApic::io_apic_count++];
			apic.base = PhysAddr {as<usize>(io_apic_addr)}.to_virt().as_usize();
			apic.int_base = global_int_base;
		}
		// IO APIC Interrupt Source Override
		else if (type == 2) {
			// bus src
			i += 1;
			u8 irq_src = *cast<const u8*>(offset(madt, as<isize>(i)));
			i += 1;
			u32 global_int = *cast<const u32*>(offset(madt, as<isize>(i)));
			i += 4;
			u16 flags = *cast<const u16*>(offset(madt, as<isize>(i)));
			i += 2;

			bool active_low = flags & 1 << 1;
			bool level_triggered = flags & 1 << 3;

			IoApic::register_override(irq_src, global_int, active_low, level_triggered);
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
			//u32 x2apic_id = *cast<const u32*>(offset(madt, as<isize>(i)));
			i += 4;
			//u32 flags = *cast<const u32*>(offset(madt, as<isize>(i)));
			i += 4;
			//u32 acpi_id = *cast<const u32*>(offset(madt, as<isize>(i)));
			i += 4;
		}
	}

	for (u8 i = 0; i < IoApic::io_apic_count; ++i) {
		auto& apic = IoApic::apics[i];
		apic.irq_count = (IoApic::read(i, 1) >> 16 & 0xFF) + 1;
	}

	Lapic::base = PhysAddr {lapic_phys}.to_virt().as_usize();

	println("assigning kb entaerp");

	IoApic::RedirEntry ps2_kb_entry {
			.vector = 0x20,
			.dest = bsp_id
	};

	IoApic::register_isa_irq(1, ps2_kb_entry);

	// enable lapic and set int vector to 0xFF
	Lapic::write(Lapic::Reg::SpuriousInt, 0xFF | 0x100);
	Lapic::write(Lapic::Reg::TaskPriority, 0);

	println("enabled");
}
