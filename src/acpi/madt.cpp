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

[[noreturn]] void ap_entry(u8 apic_id) {
	println("cpu ", apic_id, " online!");
	while (true) {
		asm volatile("hlt");
	}
}

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

		// Processor Local APIC
		if (type == 0) {
			u8 acpi_cpu_id = *cast<const u8*>(offset(madt, as<isize>(i)));
			i += 1;
			u8 apic_id = *cast<const u8*>(offset(madt, as<isize>(i)));
			i += 1;
			u32 flags = *cast<const u32*>(offset(madt, as<isize>(i)));
			i += 4;

			++cpu_count;
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

	auto lapic_phys_addr = PhysAddr {lapic_phys};
	get_map()->map_multiple(
			lapic_phys_addr.to_virt(),
			lapic_phys_addr,
			PageFlags::Rw | PageFlags::Nx | PageFlags::Huge | PageFlags::CacheDisable,
			1);

	Lapic::base = lapic_phys_addr.to_virt().as_usize();

	println("assigning kb entaerp");

	IoApic::RedirEntry ps2_kb_entry {
			.vector = 0x21,
			.dest = bsp_id
	};

	IoApic::register_isa_irq(1, ps2_kb_entry);

	// enable lapic and set int vector to 0xFF
	Lapic::write(Lapic::Reg::SpuriousInt, 0xFF | 0x100);
	Lapic::write(Lapic::Reg::TaskPriority, 0);

	println("enabled");

	return;

	if (cpu_count == 1) {
		return;
	}

	struct [[gnu::packed]] InitInfo {
		void* entry;
		void* stack;
		u32 page_table;
		u8 apic_id;
		u8 done;
	};

	extern char smp_tramp_start[];
	extern char smp_tramp_end[];

	constexpr u32 DEST_MODE_INIT = 5 << 8;
	constexpr u32 DEST_MODE_SIPI = 6 << 8;
	constexpr u32 DELIVERY_STATUS = 1 << 12;
	constexpr u32 INIT_DEASSERT_CLEAR = 1 << 14;
	constexpr u32 INIT_DEASSERT = 1 << 15;

	usize tramp_size = smp_tramp_end - smp_tramp_start;

	void* tramp_base = ALLOCATOR.alloc_low(tramp_size + sizeof(InitInfo));
	auto phys = VirtAddr {tramp_base}.to_phys();
	if (phys.as_usize() > SIZE_2MB / 2) {
		panic("smp tramp is above 1mb (");
	}
	auto addr = VirtAddr {phys.as_usize()};
	get_map()->map(addr, phys, PageFlags::Rw | PageFlags::Huge);
	memcpy(tramp_base, smp_tramp_start, tramp_size);

	u8* tramp_ptr = cast<u8*>(tramp_base);
	for (usize i = 0; i < tramp_size; ++i) {
		if (*cast<u32*>(tramp_ptr + i) == 0xCAFEBABE) {
			*cast<u32*>(tramp_ptr + i) = as<u32>(phys.as_usize());
			println("patched address");
			break;
		}
	}

	volatile auto* info = new (cast<void*>(cast<usize>(tramp_base) + tramp_size)) InitInfo();
	auto map_phys = VirtAddr {get_map()}.to_phys().as_usize();
	info->page_table = as<u32>(map_phys);
	info->entry = cast<void*>(ap_entry);

	for (usize i = 0; i < cpu_count; ++i) {
		if (i == bsp_id) {
			continue;
		}

		u32 value = DEST_MODE_INIT | INIT_DEASSERT_CLEAR;
		info->apic_id = i;
		info->stack = cast<void*>(cast<usize>(PAGE_ALLOCATOR.alloc(2)) + 0x4000);

		Lapic::write(as<Lapic::Reg>(as<u32>(Lapic::Reg::IntCmdBase) + 0x10), i << 24);
		Lapic::write(Lapic::Reg::IntCmdBase, value);
		while (Lapic::read(Lapic::Reg::IntCmdBase) & DELIVERY_STATUS) asm volatile("pause" : : : "memory");
		println("after first write");

		value = DEST_MODE_INIT | INIT_DEASSERT;
		Lapic::write(as<Lapic::Reg>(as<u32>(Lapic::Reg::IntCmdBase) + 0x10), i << 24);
		Lapic::write(Lapic::Reg::IntCmdBase, value);
		while (Lapic::read(Lapic::Reg::IntCmdBase) & DELIVERY_STATUS) asm volatile("pause" : : : "memory");
		println("after second write");

		udelay(10 * 1000);
		println("after delay");

		for (u8 j = 0; j < 2; ++j) {
			value = 1 | DEST_MODE_SIPI | INIT_DEASSERT_CLEAR;
			Lapic::write(as<Lapic::Reg>(as<u32>(Lapic::Reg::IntCmdBase) + 0x10), i << 24);
			Lapic::write(Lapic::Reg::IntCmdBase, value);

			udelay(j == 0 ? 1000 : 1000 * 1000);
			println("after third write");

			while (Lapic::read(Lapic::Reg::IntCmdBase) & DELIVERY_STATUS) asm volatile("pause" : : : "memory");
			println("after fourth write");

			if (info->done) {
				break;
			}
		}

		if (info->done) {
			println("info done");
			info->done = false;
		}
		else {
			PAGE_ALLOCATOR.dealloc(cast<void*>(cast<usize>(info->stack) - 0x2000), 2);
		}
	}
}
