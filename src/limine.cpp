#include "limine/limine.h"
#include "arch.hpp"
#include "console.hpp"
#include "cpu/cpu.hpp"
#include "memory/map.hpp"
#include "memory/memory.hpp"
#include "memory/pmm.hpp"
#include "memory/vmem.hpp"
#include "new.hpp"
#include "noalloc/string.hpp"
#include "types.hpp"

static limine_framebuffer_request FB_REQUEST {.id = LIMINE_FRAMEBUFFER_REQUEST};
static limine_hhdm_request HHDM_REQUEST {.id = LIMINE_HHDM_REQUEST};
static limine_memmap_request MEMMAP_REQUEST {.id = LIMINE_MEMMAP_REQUEST};
static limine_module_request MODULE_REQUEST {.id = LIMINE_MODULE_REQUEST};
static limine_rsdp_request RSDP_REQUEST {.id = LIMINE_RSDP_REQUEST};
static limine_kernel_address_request KERNEL_ADDRESS_REQUEST {.id = LIMINE_KERNEL_ADDRESS_REQUEST};
static limine_smp_request SMP_REQUEST {.id = LIMINE_SMP_REQUEST};

static PageMap* kernel_pmap {};

[[noreturn]] static void arch_ap_entry(limine_smp_info* info) {
	if (!kernel_pmap) {
		panic("kernel_pmap is null");
	}

	kernel_pmap->load();

	println("creating cpu local");
	auto data = new CpuLocal(info->lapic_id);
	println("cpu local created, loading gdt");
	load_gdt(data->gdt, 7);
	println("gdt loaded");
	set_cpu_local(data);
	asm volatile("mov ax, 0x28; ltr ax" : : : "ax");
	println("enabling interrupts");
	set_exceptions();
	load_idt(&data->idt);
	enable_interrupts();

	println("cpu ", info->lapic_id, " online!");

	auto fn = cast<__attribute__((noreturn)) void (*)(u8)>(info->extra_argument);
	fn(info->lapic_id);
}

void arch_init_smp(__attribute__((noreturn)) void (*fn)(u8 id)) {
	for (usize i = 0; i < SMP_REQUEST.response->cpu_count; ++i) {
		auto& cpu = SMP_REQUEST.response->cpus[i];
		if (cpu->lapic_id == SMP_REQUEST.response->bsp_lapic_id) {
			continue;
		}

		cpu->extra_argument = cast<u64>(fn);
		cpu->goto_address = arch_ap_entry;
	}
}

void* arch_get_rsdp() {
	return RSDP_REQUEST.response->address;
}

const PsfFont* arch_get_font() {
	const PsfFont* font = nullptr;
	for (usize i = 0; i < MODULE_REQUEST.response->module_count; ++i) {
		auto module = MODULE_REQUEST.response->modules[i];
		if (noalloc::String(module->cmdline) == "Tamsyn8x16r.psf") {
			font = cast<const PsfFont*>(module->address);
		}
	}

	return font;
}

Framebuffer arch_get_framebuffer() {
	auto fb = FB_REQUEST.response->framebuffers[0];
	Framebuffer f {cast<usize>(fb->address), fb->width, fb->height, fb->pitch, as<usize>(fb->bpp / 8)};
	return f;
}

void arch_init_mem() {
	HHDM_OFFSET = HHDM_REQUEST.response->offset;

	auto kernel_virt = KERNEL_ADDRESS_REQUEST.response->virtual_base;
	auto kernel_phys = KERNEL_ADDRESS_REQUEST.response->physical_base;

	auto max_phys_entry = MEMMAP_REQUEST.response->entries[MEMMAP_REQUEST.response->entry_count - 1];
	auto max_phys = max_phys_entry->base + max_phys_entry->length;

	for (usize i = 0; i < MEMMAP_REQUEST.response->entry_count; ++i) {
		auto entry = MEMMAP_REQUEST.response->entries[i];
		if (entry->type == LIMINE_MEMMAP_USABLE) {
			PAGE_ALLOCATOR.add_mem(entry->base, entry->length);
		}
	}

	if (max_phys < SIZE_4GB) {
		max_phys = SIZE_4GB;
	}
	auto aligned_max_phys = ALIGNUP(max_phys, SIZE_2MB);

	vm_kernel_init(HHDM_OFFSET + aligned_max_phys, 0xFFFFFFFF80000000 - (HHDM_OFFSET + aligned_max_phys));

	auto page_map_mem = PAGE_ALLOCATOR.alloc_new();
	if (!page_map_mem) {
		panic("failed to allocate memory for page map");
	}
	auto page_map = new (PhysAddr {page_map_mem}.to_virt()) PageMap;

	for (usize i = 0; i < SIZE_4GB; i += SIZE_2MB) {
		auto phys = PhysAddr {i};
		page_map->map(phys.to_virt(), phys, PageFlags::Rw | PageFlags::Huge);
	}

	for (usize i = 0; i < MEMMAP_REQUEST.response->entry_count; ++i) {
		auto entry = MEMMAP_REQUEST.response->entries[i];

		if (entry->base + entry->length < SIZE_4GB) {
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

	kernel_pmap = page_map;
}