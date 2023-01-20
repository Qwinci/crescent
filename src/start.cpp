#include "acpi/common.hpp"
#include "acpi/lapic.hpp"
#include "console.hpp"
#include "cpu/cpu.hpp"
#include "drivers/pit.hpp"
#include "fb.hpp"
#include "limine/limine.h"
#include "memory/map.hpp"
#include "memory/memory.hpp"
#include "new.hpp"
#include "noalloc/string.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "drivers/hpet.hpp"

static limine_framebuffer_request FB_REQUEST {.id = LIMINE_FRAMEBUFFER_REQUEST};

static limine_hhdm_request HHDM_REQUEST {.id = LIMINE_HHDM_REQUEST};

static limine_memmap_request MEMMAP_REQUEST {.id = LIMINE_MEMMAP_REQUEST};

static limine_module_request MODULE_REQUEST {.id = LIMINE_MODULE_REQUEST};

static limine_rsdp_request RSDP_REQUEST {.id = LIMINE_RSDP_REQUEST};

static limine_kernel_address_request KERNEL_ADDRESS_REQUEST {.id = LIMINE_KERNEL_ADDRESS_REQUEST};

[[gnu::used]] u8 stack[0x2000];

enum class MemoryType : u8 {
	Usable,
	Reserved,
	AcpiReclaimable,
	AcpiNvs,
	KernelAndModules,
	Framebuffer
};

struct MemoryMap {
	struct Entry {
		usize base;
		usize size;
		MemoryType type;
	};
	usize entry_count;
	Entry* entries;
};

extern "C" [[noreturn, gnu::naked, gnu::used]] void start() {
	asm volatile("lea rsp, [stack + 0x2000]; jmp kstart");
}

using fn = void (*)();

extern "C" fn __init_array_start[];
extern "C" fn __init_array_end[];
extern "C" fn __fini_array_start[];
extern "C" fn __fini_array_end[];

[[gnu::interrupt]] void dummy(InterruptFrame*) {
	println("timer tick");
	Lapic::write(Lapic::Reg::Eoi, 0);
}

extern "C" [[noreturn, gnu::used]] void kstart() {
	for (fn* f = __init_array_start; f != __init_array_end; ++f) {
		(*f)();
	}

	HHDM_OFFSET = HHDM_REQUEST.response->offset;

	auto rsdp = RSDP_REQUEST.response->address;
	auto kernel_virt = KERNEL_ADDRESS_REQUEST.response->virtual_base;
	auto kernel_phys = KERNEL_ADDRESS_REQUEST.response->physical_base;

	auto fb = FB_REQUEST.response->framebuffers[0];

	const PsfFont* font = nullptr;
	for (usize i = 0; i < MODULE_REQUEST.response->module_count; ++i) {
		auto module = MODULE_REQUEST.response->modules[i];
		if (noalloc::String(module->cmdline) == "Tamsyn8x16r.psf") {
			font = cast<const PsfFont*>(module->address);
		}
	}

	Framebuffer f {cast<usize>(fb->address), fb->width, fb->height, fb->pitch, as<usize>(fb->bpp / 8)};
	init_console(&f, font);

	set_fg(0x00FF00);

	for (usize i = 0; i < MEMMAP_REQUEST.response->entry_count; ++i) {
		auto entry = MEMMAP_REQUEST.response->entries[i];
		if (entry->type == LIMINE_MEMMAP_USABLE) {
			PAGE_ALLOCATOR.add_memory(entry->base + HHDM_OFFSET, entry->length);
		}
	}

	auto data = new CpuLocal;
	set_cpu_local(data);
	load_gdt(data->gdt, 7);
	asm volatile("mov ax, 0x28; ltr ax" : : : "ax");
	println("enabling interrupts");
	load_idt(&data->idt);
	enable_interrupts();

	auto page_map = new (PAGE_ALLOCATOR.alloc_low(1)) PageMap;

	for (usize i = 0; i < 0x100000000; i += SIZE_2MB) {
		auto phys = PhysAddr {i};
		page_map->map(phys.to_virt(), phys, PageFlags::Rw | PageFlags::Huge);
	}

	auto* entries = new MemoryMap::Entry[MEMMAP_REQUEST.response->entry_count];
	usize entry_count = 0;

	for (usize i = 0; i < MEMMAP_REQUEST.response->entry_count; ++i) {
		auto entry = MEMMAP_REQUEST.response->entries[i];

		if (entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
			auto& copy_entry = entries[entry_count++];
			copy_entry.base = entry->base;
			copy_entry.type = MemoryType::Usable;
			copy_entry.size = entry->length;
		}

		if (entry->type != LIMINE_MEMMAP_USABLE &&
			entry->type != LIMINE_MEMMAP_FRAMEBUFFER &&
			entry->type != LIMINE_MEMMAP_KERNEL_AND_MODULES) {
			continue;
		}

		usize align = 0;
		if (entry->base & (0x1000 - 1)) {
			align = entry->base & (0x1000 - 1);
			entry->base &= 0x1000 - 1;
		}

		usize j = 0;
		usize size = entry->length + align;
		while ((entry->base + j) & (SIZE_2MB - 1) && j < size) {
			auto phys = PhysAddr {entry->base + j};
			page_map->map(phys.to_virt(), phys, PageFlags::Rw);
			j += 0x1000;
		}

		while (j < size) {
			auto phys = PhysAddr {entry->base + j};
			page_map->map(phys.to_virt(), phys, PageFlags::Rw | PageFlags::Huge);
			j += SIZE_2MB;
		}
	}


	for (usize i = 0; i < entry_count; ++i) {
		const auto& entry = entries[i];
		if (entry.type == MemoryType::Usable) {
			PAGE_ALLOCATOR.add_memory(entry.base + HHDM_OFFSET, entry.size);
		}
	}

	ALLOCATOR.dealloc(entries, entry_count * sizeof(MemoryMap::Entry));

	extern char KERNEL_START[];
	extern char KERNEL_END[];

	auto size = KERNEL_END - KERNEL_START;

	usize i = 0;
	while ((kernel_phys + i) & (SIZE_2MB - 1) && i < size) {
		page_map->map(VirtAddr {kernel_virt + i}, PhysAddr {kernel_phys + i}, PageFlags::Rw);
		i += 0x1000;
	}

	while (i < size) {
		page_map->map(VirtAddr {kernel_virt + i}, PhysAddr {kernel_phys + i}, PageFlags::Rw | PageFlags::Huge);
		i += SIZE_2MB;
	}

	page_map->load();

	println("hello");

	parse_madt(locate_acpi_table(rsdp, "APIC"));

	data->idt.interrupts[0] = {(void*) dummy, 0x8, 0, false, 0};

	initialize_hpet(locate_acpi_table(rsdp, "HPET"));

	constexpr u32 ONE_SHOT = 0 << 17;
	constexpr u32 PERIODIC = 1 << 17;
	constexpr u32 TSC = 2 << 17;

	u32 eax;
	asm volatile("cpuid" : "=a"(eax) : "a"(0x80000000));
	if (eax >= 80000007) {
		u32 edx;
		asm volatile("cpuid" : "=d"(edx) : "a"(0x80000007));
		bool invariant_tsc = edx & 1 << 8;
		println("invariant tsc supported: ", invariant_tsc);
		if (invariant_tsc) {
			println("doing tsc frequency test");
			u32 start_low, start_high;
			u32 end_low, end_high;
			asm volatile("rdtsc" : "=a"(start_low), "=d"(start_high));
			hpet_sleep(10 * 1000);
			asm volatile("rdtsc" : "=a"(end_low), "=d"(end_high));
			auto start = (u64) start_low | (u64) start_high << 32;
			auto end = (u64) end_low | (u64) end_high << 32;
			auto diff = end - start;
			auto tsc_ticks_in_sec = diff * 100;
			constexpr u64 GHZ = 1000000000;
			println("tsc frequency: ", tsc_ticks_in_sec, "hz", " (", tsc_ticks_in_sec / GHZ, "ghz)");

			println("doing tsc sleep for 10s");
			u32 low, high;
			u64 full;
			asm volatile("rdtsc" : "=a"(low), "=d"(high));
			start = (u64) low | (u64) high << 32;
			end = start + tsc_ticks_in_sec * 10;
			while (true) {
				asm volatile("rdtsc" : "=a"(low), "=d"(high));
				full = (u64) low | (u64) high << 32;
				if (full >= end) break;
			}
			println("tsc sleep done");
		}
	}

	pit_prepare_sleep();

	Lapic::write(Lapic::Reg::LvtTimer, 32 | ONE_SHOT | Lapic::INT_MASKED);
	// 16
	Lapic::write(Lapic::Reg::DivConf, 3);
	Lapic::write(Lapic::Reg::InitCount, 0xFFFFFFFF);
	auto value = Lapic::read(Lapic::Reg::LvtTimer) & ~Lapic::INT_MASKED;
	Lapic::write(Lapic::Reg::LvtTimer, value);

	//hpet_sleep(10 * 1000);
	pit_perform_10ms_sleep();

	auto end = Lapic::read(Lapic::Reg::CurrCount);
	Lapic::write(Lapic::Reg::LvtTimer, Lapic::INT_MASKED);

	auto ticks_in_10ms = (0xFFFFFFFF - end);
	auto ticks_in_1s = ticks_in_10ms * 100;
	auto ticks_in_1ms = ticks_in_10ms / 10;
	auto ticks_in_1us = ticks_in_1ms / 1000;
	auto ticks_in_100ns = ticks_in_1us / 10;
	println("ticks in 10ms with divider 16: ", ticks_in_10ms);
	println("ticks in 1ms with divider 16: ", ticks_in_1ms);
	println("ticks in 100ns with divider 16: ", ticks_in_100ns);

	println("sleeping for 10s using the apic");
	Lapic::write(Lapic::Reg::InitCount, ticks_in_1s * 10);
	Lapic::write(Lapic::Reg::LvtTimer, 32 | ONE_SHOT);

	while (true) {
		asm("hlt");
	}
}