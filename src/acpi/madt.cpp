#include "common.hpp"
#include "console.hpp"
#include "io.hpp"
#include "io_apic.hpp"
#include "lapic.hpp"
#include "memory/map.hpp"
#include "memory/memory.hpp"
#include "memory/std.hpp"
#include "new.hpp"
#include "timer/timer.hpp"
#include "utils.hpp"

struct Madt {
	SdtHeader header;
	u32 lapic_addr;
	u32 flags;
};

void parse_madt(const void* madt_ptr) {
	usize cpu_count = 0;

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

		// IO APIC
		if (type == 1) {
			// io_apic_id
			i += 1;
			// reserved
			i += 1;
			u32 io_apic_addr = *cast<const u32*>(offset(madt, as<isize>(i)));
			i += 4;
			u32 global_int_base = *cast<const u32*>(offset(madt, as<isize>(i)));
			i += 4;

			auto& apic = IoApic::apics[IoApic::io_apic_count++];
			auto addr = PhysAddr {as<usize>(io_apic_addr)};
			apic.base = addr.to_virt().as_usize();
			apic.int_base = global_int_base;
			get_map()->map_multiple(
					addr.to_virt(),
					addr,
					PageFlags::Rw | PageFlags::Nx | PageFlags::Huge | PageFlags::CacheDisable,
					1);
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
	}

	for (u8 i = 0; i < IoApic::io_apic_count; ++i) {
		auto& apic = IoApic::apics[i];
		apic.irq_count = (IoApic::read(i, 1) >> 16 & 0xFF) + 1;
	}

	auto lapic_phys_addr = PhysAddr {lapic_phys};
	get_map()->map(
			lapic_phys_addr.to_virt(),
			lapic_phys_addr,
			PageFlags::Rw | PageFlags::Nx | PageFlags::Huge | PageFlags::CacheDisable);

	Lapic::base = lapic_phys_addr.to_virt().as_usize();

	println("assigning kb entaerp");

	/*IoApic::RedirEntry ps2_kb_entry {
			.vector = 0x21,
			.dest = bsp_id
	};

	IoApic::register_isa_irq(1, ps2_kb_entry);*/

	Lapic::init();
}
