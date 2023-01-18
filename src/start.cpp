#include "acpi/common.hpp"
#include "console.hpp"
#include "cpu/cpu.hpp"
#include "fb.hpp"
#include "limine/limine.h"
#include "memory/map.hpp"
#include "memory/memory.hpp"
#include "new.hpp"
#include "noalloc/string.hpp"
#include "types.hpp"
#include "utils.hpp"

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

	auto page_map = new (PAGE_ALLOCATOR.alloc_low(1)) PageMap;

	for (usize i = 0; i < 0x100000000; i += SIZE_2MB) {
		auto phys = PhysAddr {i};
		page_map->map(phys.to_virt(), phys, PageFlags::Rw | PageFlags::Huge);
	}

	for (usize i = 0; i < MEMMAP_REQUEST.response->entry_count; ++i) {
		auto entry = MEMMAP_REQUEST.response->entries[i];

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

	auto data = new CpuLocal;
	set_cpu_local(data);
	load_gdt(data->gdt, 7);
	asm volatile("mov ax, 0x28; ltr ax" : : : "ax");
	println("enabling interrupts");
	load_idt(&data->idt);

	println("hello");

	println((void*) locate_acpi_table(rsdp, "APIC"));

	parse_madt(locate_acpi_table(rsdp, "APIC"));

	while (true) {
		asm("hlt");
	}
}