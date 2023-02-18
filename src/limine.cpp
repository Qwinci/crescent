#include "limine/limine.h"
#include "acpi/lapic.hpp"
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

static volatile limine_framebuffer_request FB_REQUEST {.id = LIMINE_FRAMEBUFFER_REQUEST};
static volatile limine_hhdm_request HHDM_REQUEST {.id = LIMINE_HHDM_REQUEST};
static volatile limine_memmap_request MEMMAP_REQUEST {.id = LIMINE_MEMMAP_REQUEST};
static volatile limine_module_request MODULE_REQUEST {.id = LIMINE_MODULE_REQUEST};
static volatile limine_rsdp_request RSDP_REQUEST {.id = LIMINE_RSDP_REQUEST};
static volatile limine_kernel_address_request KERNEL_ADDRESS_REQUEST {.id = LIMINE_KERNEL_ADDRESS_REQUEST};
static volatile limine_smp_request SMP_REQUEST {.id = LIMINE_SMP_REQUEST};

static atomic_uint_fast8_t cpus_init = 0;
static Spinlock smp_lock {};
extern PageMap* kernel_map;

struct StartInfo {
	u8 lapic_id;
	u32 acpi_id;
	__attribute__((noreturn)) void (*fn)(u8, u32);
	u8* stack;
};

[[noreturn]] void arch_ap_entry(StartInfo* info);

[[noreturn]] void arch_ap_entry_start(limine_smp_info* info) {
	if (!kernel_map) {
		panic("kernel_pmap is null");
	}

	auto start_info = cast<StartInfo*>(info->extra_argument);

	kernel_map->load();

	asm volatile(R"(
		mov rdi, %0;
		mov rsp, %1;
		xor rbp, rbp;
		jmp %2;)" :
				 : "rm"(start_info),
				   "rm"(start_info->stack),
				   "A"(&arch_ap_entry));
	__builtin_unreachable();
}

usize cpu_count = 1;
CpuLocal* cpu_locals;

[[noreturn]] void arch_ap_entry(StartInfo* info) {
	smp_lock.lock();

	cpu_locals[cpu_count++].id = info->lapic_id;
	load_gdt(&cpu_locals[cpu_count - 1].tss);
	set_cpu_local(&cpu_locals[cpu_count - 1]);

	smp_lock.unlock();

	asm volatile("mov ax, 6 * 8; ltr ax" : : : "ax");
	set_exceptions();
	load_idt();
	enable_interrupts();

	Lapic::init();

	smp_lock.lock();

	// cpu_locals[cpu_count++].id = cpuid(1).ebx >> 24;

	Lapic::calibrate_timer();
	smp_lock.unlock();

	auto fn = info->fn;
	auto lapic_id = info->lapic_id;
	auto acpi_id = info->acpi_id;

	ALLOCATOR.dealloc(info, sizeof(*info));

	atomic_fetch_add_explicit(&cpus_init, 1, memory_order_relaxed);

	fn(lapic_id, acpi_id);
}

void arch_init_cpu_locals() {
	cpu_locals = new CpuLocal[SMP_REQUEST.response->cpu_count]();

	auto data = &cpu_locals[0];
	load_gdt(&data->tss);
	set_cpu_local(data);
	asm volatile("mov ax, 6 * 8; ltr ax" : : : "ax");
	set_exceptions();
	load_idt();
	enable_interrupts();
}

void arch_init_smp(void (*fn)(u8 id, u32 acpi_id)) {
	for (usize i = 0; i < SMP_REQUEST.response->cpu_count; ++i) {
		auto& cpu = SMP_REQUEST.response->cpus[i];
		if (cpu->lapic_id == SMP_REQUEST.response->bsp_lapic_id) {
			continue;
		}

		u8* stack = new u8[0x2000]();
		stack += 0x2000;
		auto data = new StartInfo {
				as<u8>(cpu->lapic_id),
				cpu->processor_id,
				cast<__attribute__((noreturn)) void (*)(u8, u32)>(fn),
				stack};

		cpu->extra_argument = cast<u64>(data);
		cpu->goto_address = arch_ap_entry_start;
	}

	while (cpus_init != SMP_REQUEST.response->cpu_count - 1) {
		__builtin_ia32_pause();
	}
}

void* arch_get_rsdp() {
	return RSDP_REQUEST.response->address;
}

void* arch_get_module(const char* name) {
	for (usize i = 0; i < MODULE_REQUEST.response->module_count; ++i) {
		auto module = MODULE_REQUEST.response->modules[i];
		if (noalloc::String(module->cmdline) == name) {
			return module->address;
		}
	}
	return nullptr;
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

	kernel_map = page_map;

	for (usize i = 0; i < SIZE_4GB; i += SIZE_2MB) {
		auto phys = PhysAddr {i};
		page_map->map(phys.to_virt(), phys, PageFlags::Rw | PageFlags::Huge);
	}

	for (usize i = 0; i < MEMMAP_REQUEST.response->entry_count; ++i) {
		auto entry = MEMMAP_REQUEST.response->entries[i];

		if (entry->type == LIMINE_MEMMAP_FRAMEBUFFER) {
			usize align = 0;
			if (entry->base & (0x1000 - 1)) {
				align = entry->base & (0x1000 - 1);
				entry->base &= 0x1000 - 1;
			}

			usize j = 0;
			usize size = entry->length + align;
			while (j < size) {
				auto phys = PhysAddr {entry->base + j};
				page_map->map(phys.to_virt(), phys, PageFlags::Rw | PageFlags::CacheDisable, true);
				j += 0x1000;
			}
		}

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

	extern char RODATA_START[];
	extern char RODATA_END[];
	extern char TEXT_START[];
	extern char TEXT_END[];
	extern char DATA_START[];
	extern char DATA_END[];

	const usize RODATA_SIZE = RODATA_END - RODATA_START;
	const usize RODATA_START_OFF = RODATA_START - cast<char*>(kernel_virt);

	const usize TEXT_SIZE = TEXT_END - TEXT_START;
	const usize TEXT_START_OFF = TEXT_START - cast<char*>(kernel_virt);

	const usize DATA_SIZE = DATA_END - DATA_START;
	const usize DATA_START_OFF = DATA_START - cast<char*>(kernel_virt);

	for (usize i = RODATA_START_OFF; i < RODATA_START_OFF + RODATA_SIZE; i += 0x1000) {
		page_map->map(VirtAddr {kernel_virt + i}, PhysAddr {kernel_phys + i}, PageFlags::Nx);
	}

	for (usize i = TEXT_START_OFF; i < TEXT_START_OFF + TEXT_SIZE; i += 0x1000) {
		page_map->map(VirtAddr {kernel_virt + i}, PhysAddr {kernel_phys + i}, PageFlags::Present);
	}

	for (usize i = DATA_START_OFF; i < DATA_START_OFF + DATA_SIZE; i += 0x1000) {
		page_map->map(VirtAddr {kernel_virt + i}, PhysAddr {kernel_phys + i}, PageFlags::Rw | PageFlags::Nx);
	}

	page_map->load();
}