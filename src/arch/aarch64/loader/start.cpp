#include "arch/aarch64/dtb.hpp"
#include "arch/paging.hpp"
#include "constants.hpp"
#include "early_paging.hpp"
#include "mem/mem.hpp"
#include "new.hpp"
#include "sched/process.hpp"

[[gnu::visibility("hidden")]] extern char TEXT_START[];
[[gnu::visibility("hidden")]] extern char TEXT_END[];
[[gnu::visibility("hidden")]] extern char RODATA_START[];
[[gnu::visibility("hidden")]] extern char RODATA_END[];
[[gnu::visibility("hidden")]] extern char DATA_START[];
[[gnu::visibility("hidden")]] extern char DATA_END[];
[[gnu::visibility("hidden")]] extern char KERNEL_START[];
[[gnu::visibility("hidden")]] extern char KERNEL_END[];

constexpr usize SIZE_GB = 1024 * 1024 * 1024;

EarlyPageMap* AARCH64_IDENTITY_MAP;
EarlyPageMap* AARCH64_EARLY_KERNEL_MAP;

[[gnu::always_inline]] static inline void setup_memory(usize max_addr) {
	auto* kernel_map = new (early_page_alloc()) EarlyPageMap;

	const usize TEXT_SIZE = TEXT_END - TEXT_START;
	const usize TEXT_OFF = TEXT_START - KERNEL_START;
	const usize RODATA_SIZE = RODATA_END - RODATA_START;
	const usize RODATA_OFF = RODATA_START - KERNEL_START;
	const usize DATA_SIZE = DATA_END - DATA_START;
	const usize DATA_OFF = DATA_START - KERNEL_START;

	for (usize i = TEXT_OFF; i < TEXT_OFF + TEXT_SIZE; i += 0x1000) {
		(void) kernel_map->map(
			KERNEL_OFFSET + i,
			reinterpret_cast<u64>(KERNEL_START) + i,
			PageFlags::Read | PageFlags::Execute,
			CacheMode::WriteBack);
	}
	for (usize i = RODATA_OFF; i < RODATA_OFF + RODATA_SIZE; i += 0x1000) {
		(void) kernel_map->map(
			KERNEL_OFFSET + i,
			reinterpret_cast<u64>(KERNEL_START) + i,
			PageFlags::Read,
			CacheMode::WriteBack);
	}
	for (usize i = DATA_OFF; i < DATA_OFF + DATA_SIZE; i += 0x1000) {
		(void) kernel_map->map(
			KERNEL_OFFSET + i,
			reinterpret_cast<u64>(KERNEL_START) + i,
			PageFlags::Read | PageFlags::Write,
			CacheMode::WriteBack);
	}

	AARCH64_IDENTITY_MAP = new (early_page_alloc()) EarlyPageMap;

	for (usize i = 0; i < max_addr; i += SIZE_GB) {
		(void) AARCH64_IDENTITY_MAP->map_1gb(i, i, PageFlags::Read | PageFlags::Write | PageFlags::Execute, CacheMode::WriteBack);
		(void) kernel_map->map_1gb(HHDM_OFFSET + i, i, PageFlags::Read | PageFlags::Write, CacheMode::WriteBack);
	}

	u64 ttbr0_el1 = reinterpret_cast<u64>(AARCH64_IDENTITY_MAP);
	u64 ttbr1_el1 = reinterpret_cast<u64>(kernel_map);
	asm volatile("msr TTBR0_EL1, %0" : : "r"(ttbr0_el1));
	asm volatile("msr TTBR1_EL1, %0" : : "r"(ttbr1_el1));

	u64 id_aa64mmfr0_el1;
	asm volatile("mrs %0, ID_AA64MMFR0_EL1" : "=r"(id_aa64mmfr0_el1));

	u64 tcr_el1 = 0; // control register
	// T0SZ
	tcr_el1 |= 64 - 48;
	// T1SZ
	tcr_el1 |= (64 - 48) << 16;
	// TG1
	tcr_el1 |= 0b10U << 30;
	// IPS
	tcr_el1 |= (id_aa64mmfr0_el1 & 0b1111) << 32;

	// IRGN0 (TTBR0 inner cacheability) write-back write-allocate cacheable
	tcr_el1 |= 0b01 << 8;
	// ORGN0 (TTBR0 outer cacheability) write-back write-allocate cacheable
	tcr_el1 |= 0b01 << 10;
	// SH0 (TTBR0 shareability) outer shareable
	tcr_el1 |= 0b10 << 12;
	// IRGN1 (TTBR1 inner cacheability) write-back write-allocate cacheable
	tcr_el1 |= 0b01 << 24;
	// ORGN1 (TTBR1 outer cacheability) write-back write-allocate cacheable
	tcr_el1 |= 1 << 26;
	// SH1 (TTBR1 shareability) outer shareable
	tcr_el1 |= 0b10 << 28;

	asm volatile("msr TCR_EL1, %0" : : "r"(tcr_el1));

	u64 mair_el1 = 0;
	// normal write-back
	mair_el1 |= 0xFF;
	// normal non-cacheable
	mair_el1 |= 0b01000100 << 8;
	// device nGnRnE

	// 0b00 == nGnRnE
	// 0b01 == nGnRE
	// 0b10 == nGRE
	// 0b11 == GRE     | type
	mair_el1 |= 0b0000'00'00U << 16;
	asm volatile("msr MAIR_EL1, %0" : : "r"(mair_el1));

	// force changes
	asm volatile("isb");

	u64 sctlr_el1;
	asm volatile("mrs %0, SCTLR_EL1" : "=r"(sctlr_el1));
	// enable mmu
	sctlr_el1 |= 1;
	// enable cache
	sctlr_el1 |= 1 << 2;
	sctlr_el1 |= 1 << 12;
	sctlr_el1 &= ~(1 << 19);
	asm volatile("tlbi vmalle1; dsb nsh; msr SCTLR_EL1, %0; isb" : : "r"(sctlr_el1) : "memory");

	extern usize HHDM_START;
	HHDM_START = HHDM_OFFSET;

	AARCH64_EARLY_KERNEL_MAP = to_virt<EarlyPageMap>(reinterpret_cast<usize>(kernel_map));
}

extern "C" [[gnu::used]] void early_start(usize dtb_phys) {
	dtb::Dtb dtb {reinterpret_cast<void*>(dtb_phys)};
	auto root = dtb.get_root();

	auto memory_opt = root.find_child("memory");
	auto memory = memory_opt.value();
	dtb::SizeAddrCells memory_cells = dtb.size_cells;
	if (auto p = memory.prop("#size-cells")) {
		memory_cells.size_cells = p.value().read<u32>(0);
	}
	if (auto p = memory.prop("#address-cells")) {
		memory_cells.addr_cells = p.value().read<u32>(0);
	}
	u32 memory_index = 0;

	usize max_addr = 0;
	while (true) {
		if (auto reg_opt = memory.reg(memory_cells, memory_index++)) {
			auto reg = reg_opt.value();
			max_addr = kstd::max(max_addr, reg.addr + reg.size);
		}
		else {
			break;
		}
	}

	setup_memory(max_addr);

	register void* x0 asm("x0") = to_virt<void>(dtb_phys);
	register auto x1 asm("x1") = reinterpret_cast<usize>(KERNEL_START);
	asm volatile(
		"sub sp, sp, %0\n"
		"add sp, sp, %1\n"

		"adrp x2, arch_start\n"
		"add x2, x2, :lo12:arch_start\n"
		"sub x2, x2, %0\n"
		"add x2, x2, %1\n"
		"br x2\n"
		: : "r"(KERNEL_START), "r"(KERNEL_OFFSET), "r"(x0), "r"(x1) : "x2");
	__builtin_unreachable();
}
