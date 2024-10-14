#include "arch/paging.hpp"
#include "arch/x86/cpu.hpp"
#include "dev/fb/fb.hpp"
#include "info.hpp"
#include "limine.h"
#include "mem/malloc.hpp"
#include "mem/mem.hpp"
#include "mem/pmalloc.hpp"
#include "mem/vspace.hpp"
#include "new.hpp"
#include "sched/process.hpp"
#include "x86/io.hpp"

LIMINE_BASE_REVISION(1)

static volatile limine_memmap_request MMAP_REQUEST {
	.id = LIMINE_MEMMAP_REQUEST,
	.revision = 0,
	.response = nullptr
};

static volatile limine_hhdm_request HHDM_REQUEST {
	.id = LIMINE_HHDM_REQUEST,
	.revision = 0,
	.response = nullptr
};

static volatile limine_kernel_address_request KERNEL_ADDR_REQUEST {
	.id = LIMINE_KERNEL_ADDRESS_REQUEST,
	.revision = 0,
	.response = nullptr
};

static volatile limine_framebuffer_request FB_REQUEST {
	.id = LIMINE_FRAMEBUFFER_REQUEST,
	.revision = 0,
	.response = nullptr
};

static volatile limine_rsdp_request RSDP_REQUEST {
	.id = LIMINE_RSDP_REQUEST,
	.revision = 0,
	.response = nullptr
};

[[gnu::visibility("hidden")]] extern char TEXT_START[];
[[gnu::visibility("hidden")]] extern char TEXT_END[];
[[gnu::visibility("hidden")]] extern char RODATA_START[];
[[gnu::visibility("hidden")]] extern char RODATA_END[];
[[gnu::visibility("hidden")]] extern char DATA_START[];
[[gnu::visibility("hidden")]] extern char DATA_END[];
[[gnu::visibility("hidden")]] extern char KERNEL_START[];
[[gnu::visibility("hidden")]] extern char KERNEL_END[];

constexpr usize SIZE_2MB = 1024 * 1024 * 2;

static void setup_pat() {
	u64 value = 0;
	// PA0 write back
	value |= 0x6;
	// PA1 write combining
	value |= 0x1 << 8;
	// PA2 write through
	value |= 0x4 << 16;
	// PA3 uncached
	value |= 0x7 << 24;

	msrs::IA32_PAT.write(value);
}

extern usize HHDM_START;

static void setup_memory(usize max_addr) {
	setup_pat();

	HHDM_START = HHDM_REQUEST.response->offset;

	const usize kernel_phys = KERNEL_ADDR_REQUEST.response->physical_base;

	for (usize i = 0; i < MMAP_REQUEST.response->entry_count; ++i) {
		const auto entry = MMAP_REQUEST.response->entries[i];
		if (entry->type == LIMINE_MEMMAP_USABLE) {
			pmalloc_add_mem(entry->base, entry->length);
		}
	}

	KERNEL_PROCESS.initialize("kernel", false, EmptyHandle {}, EmptyHandle {}, EmptyHandle {});
	auto& KERNEL_MAP = KERNEL_PROCESS->page_map;
	KERNEL_MAP.fill_high_half();

	const usize TEXT_SIZE = TEXT_END - TEXT_START;
	const usize TEXT_OFF = TEXT_START - KERNEL_START;
	const usize RODATA_SIZE = RODATA_END - RODATA_START;
	const usize RODATA_OFF = RODATA_START - KERNEL_START;
	const usize DATA_SIZE = DATA_END - DATA_START;
	const usize DATA_OFF = DATA_START - KERNEL_START;

	for (usize i = TEXT_OFF; i < TEXT_OFF + TEXT_SIZE; i += 0x1000) {
		(void) KERNEL_MAP.map(
			reinterpret_cast<u64>(KERNEL_START) + i,
			kernel_phys + i,
			PageFlags::Read | PageFlags::Execute,
			CacheMode::WriteBack);
	}
	for (usize i = RODATA_OFF; i < RODATA_OFF + RODATA_SIZE; i += 0x1000) {
		(void) KERNEL_MAP.map(
			reinterpret_cast<u64>(KERNEL_START) + i,
			kernel_phys + i,
			PageFlags::Read,
			CacheMode::WriteBack);
	}
	for (usize i = DATA_OFF; i < DATA_OFF + DATA_SIZE; i += 0x1000) {
		(void) KERNEL_MAP.map(
			reinterpret_cast<u64>(KERNEL_START) + i,
			kernel_phys + i,
			PageFlags::Read | PageFlags::Write,
			CacheMode::WriteBack);
	}

	for (usize i = 0; i < max_addr; i += SIZE_2MB) {
		(void) KERNEL_MAP.map_2mb(HHDM_START + i, i, PageFlags::Read | PageFlags::Write, CacheMode::WriteBack);
	}

	KERNEL_MAP.use();
}

using Fn = void (*)();
extern Fn INIT_ARRAY_START[];
extern Fn INIT_ARRAY_END[];

static void run_constructors() {
	for (Fn* fn = INIT_ARRAY_START; fn != INIT_ARRAY_END; ++fn) {
		(*fn)();
	}
}

extern "C" [[noreturn]] void arch_start(BootInfo info);

struct SerialLog : LogSink {
	void write(kstd::string_view str) override {
		for (auto c : str) {
			x86::out1(0x3F8, c);
		}
	}
};
ManuallyDestroy<SerialLog> SERIAL_LOG {};

extern "C" [[noreturn, gnu::used]] void early_start() {
	run_constructors();

	LOG.lock()->register_sink(&*SERIAL_LOG);

	usize max_addr = 0;
	for (usize i = 0; i < MMAP_REQUEST.response->entry_count; ++i) {
		const auto entry = MMAP_REQUEST.response->entries[i];
		max_addr = kstd::max(max_addr, entry->base + entry->length);
	}

	setup_memory(max_addr);

	auto kernel_vm_start = ALIGNUP(HHDM_START + max_addr, 1024ULL * 1024ULL * 1024ULL);
	KERNEL_VSPACE.init(kernel_vm_start, reinterpret_cast<usize>(KERNEL_START) - kernel_vm_start);

	if (FB_REQUEST.response && FB_REQUEST.response->framebuffer_count) {
		const auto& limine_fb = FB_REQUEST.response->framebuffers[0];
		BOOT_FB.initialize(
			to_phys(limine_fb->address),
			static_cast<u32>(limine_fb->width),
			static_cast<u32>(limine_fb->height),
			limine_fb->pitch,
			limine_fb->bpp
		);
	}

	BootInfo info {
		.rsdp = RSDP_REQUEST.response->address
	};

	arch_start(info);
}
