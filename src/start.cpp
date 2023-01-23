#include "acpi/common.hpp"
#include "acpi/lapic.hpp"
#include "console.hpp"
#include "cpu/cpu.hpp"
#include "fb.hpp"
#include "limine/limine.h"
#include "memory/map.hpp"
#include "memory/memory.hpp"
#include "new.hpp"
#include "noalloc/string.hpp"
#include "timer/timer.hpp"
#include "types.hpp"
#include "utils.hpp"

static limine_framebuffer_request FB_REQUEST {.id = LIMINE_FRAMEBUFFER_REQUEST};

static limine_hhdm_request HHDM_REQUEST {.id = LIMINE_HHDM_REQUEST};

static limine_memmap_request MEMMAP_REQUEST {.id = LIMINE_MEMMAP_REQUEST};

static limine_module_request MODULE_REQUEST {.id = LIMINE_MODULE_REQUEST};

static limine_rsdp_request RSDP_REQUEST {.id = LIMINE_RSDP_REQUEST};

static limine_kernel_address_request KERNEL_ADDRESS_REQUEST {.id = LIMINE_KERNEL_ADDRESS_REQUEST};

static limine_smp_request SMP_REQUEST {.id = LIMINE_SMP_REQUEST};

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

[[noreturn]] void ap_entry(limine_smp_info* info) {
	println("cpu ", info->lapic_id, " online!");
	while (true) {
		asm("hlt");
	}
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

	for (usize i = 0; i < SMP_REQUEST.response->cpu_count; ++i) {
		auto& cpu = SMP_REQUEST.response->cpus[i];
		if (cpu->lapic_id == SMP_REQUEST.response->bsp_lapic_id) {
			continue;
		}

		cpu->goto_address = ap_entry;
	}

	for (usize i = 0; i < MEMMAP_REQUEST.response->entry_count; ++i) {
		auto entry = MEMMAP_REQUEST.response->entries[i];
		if (entry->type == LIMINE_MEMMAP_USABLE) {
			PAGE_ALLOCATOR.add_memory(entry->base + HHDM_OFFSET, entry->length);
		}
	}

	auto data = new CpuLocal;
	load_gdt(data->gdt, 7);
	set_cpu_local(data);
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
	usize max_entry_count = MEMMAP_REQUEST.response->entry_count;
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

	ALLOCATOR.dealloc(entries, max_entry_count * sizeof(MemoryMap::Entry));

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

	init_timers(rsdp);

	parse_madt(locate_acpi_table(rsdp, "APIC"));

	Lapic::calibrate_timer();

	println("hello");

	while (true) {
		asm("hlt");
	}
}