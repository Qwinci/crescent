#include "arch/aarch64/dev/discovery.hpp"
#include "arch/aarch64/loader/early_paging.hpp"
#include "arch/aarch64/mem/aarch64_mem.hpp"
#include "dtb.hpp"
#include "mem/mem.hpp"
#include "mem/pmalloc.hpp"
#include "mem/vspace.hpp"
#include "sched/process.hpp"
#include "smp.hpp"
#include "stdio.hpp"

#if BOARD_MSM
#include "dev/fb/fb.hpp"
#endif

#ifdef BOARD_QEMU_VIRT
volatile uint8_t* qemu_uart = reinterpret_cast<uint8_t*>(0xFFFF000009000000);
void uart_puts(kstd::string_view str) {
	for (auto c : str) {
		*qemu_uart = c;
	}
}
#elif defined(BOARD_MSM)
void uart_puts(kstd::string_view str) {}
#endif

void uart_puts(usize num) {
	char buf[20];
	char* ptr = buf + 20;
	do {
		*--ptr = '0' + (num % 10);
		num /= 10;
	} while (num);
	uart_puts(kstd::string_view {ptr, static_cast<size_t>(buf + 20 - ptr)});
}

void uart_hex(usize num) {
	char buf[16];
	char* ptr = buf + 16;
	static constexpr char HEX_CHARS[] = "0123456789ABCDEF";
	do {
		*--ptr = HEX_CHARS[num % 16];
		num /= 16;
	} while (num);
	uart_puts(kstd::string_view {ptr, static_cast<size_t>(buf + 16 - ptr)});
}

[[gnu::visibility("hidden")]] extern char KERNEL_START[];
[[gnu::visibility("hidden")]] extern char KERNEL_END[];

struct UartSink : LogSink {
	void write(kstd::string_view str) override {
		uart_puts(str);
	}
};

static UartSink BOOT_SINK;

extern char EXCEPTION_HANDLERS_START[];
extern usize HHDM_START;

using Fn = void (*)();
extern Fn INIT_ARRAY_START[];
extern Fn INIT_ARRAY_END[];

static void run_constructors() {
	for (Fn* fn = INIT_ARRAY_START; fn != INIT_ARRAY_END; ++fn) {
		(*fn)();
	}
}

[[noreturn]] void kmain(const void* initrd, usize size);

constexpr usize KERNEL_SIZE_ALIGN = 1024 * 1024 * 128;

extern void kernel_dtb_init(dtb::Dtb plain_dtb);

extern EarlyPageMap* AARCH64_EARLY_KERNEL_MAP;

extern "C" [[noreturn, gnu::used]] void arch_start(void* dtb_ptr, usize kernel_phys) {
	run_constructors();

	println("arch start begin");
	asm volatile("msr VBAR_EL1, %0" : : "r"(EXCEPTION_HANDLERS_START));

	// bit0 == fiq, bit1 == irq, bit2 == SError, bit3 == Debug
	asm volatile("msr DAIFClr, #0b1111");

	//asm volatile("msr TTBR0_EL1, %0" : : "r"(0ULL));
	//asm volatile("tlbi vmalle1; dsb ish; isb" : : : "memory");

	dtb::Dtb dtb {dtb_ptr};
	auto root = dtb.get_root();

	MemReserve mem_reserve[64] {};
	u32 mem_reserve_count = 0;

	usize aligned_kernel_size = (KERNEL_END - KERNEL_START + KERNEL_SIZE_ALIGN - 1) & ~(KERNEL_SIZE_ALIGN - 1);
	mem_reserve[mem_reserve_count++] = {
		.base = kernel_phys,
		.len = aligned_kernel_size
	};
	mem_reserve[mem_reserve_count++] = {
		.base = to_phys(dtb_ptr) & ~(0x1000 - 1),
		.len = (dtb.total_size + 0x1000 - 1) & ~(0x1000 - 1)
	};

	for (auto& reserved : dtb.get_reserved()) {
		mem_reserve[mem_reserve_count++] = {
			.base = kstd::byteswap(reserved.addr) & ~(0x1000 - 1),
			.len = (kstd::byteswap(reserved.size) + 0x1000 - 1) & ~(0x1000 - 1)
		};
	}

	void* initrd = nullptr;
	usize initrd_size = 0;

	if (auto chosen_opt = root.find_child("chosen")) {
		auto chosen = chosen_opt.value();
		u64 initrd_start = 0;
		if (auto initrd_start_opt = chosen.prop("linux,initrd-start")) {
			auto initrd_start_prop = initrd_start_opt.value();
			if (initrd_start_prop.len == 4) {
				initrd_start = initrd_start_prop.read<u32>(0);
			}
			else if (initrd_start_prop.len == 8) {
				initrd_start = initrd_start_prop.read<u64>(0);
			}
		}

		u64 initrd_end = 0;
		if (auto initrd_end_opt = chosen.prop("linux,initrd-end")) {
			auto initrd_end_prop = initrd_end_opt.value();
			if (initrd_end_prop.len == 4) {
				initrd_end = initrd_end_prop.read<u32>(0);
			}
			else if (initrd_end_prop.len == 8) {
				initrd_end = initrd_end_prop.read<u64>(0);
			}
		}

		if (initrd_start && initrd_end) {
			mem_reserve[mem_reserve_count++] = {
				.base = initrd_start & ~(0x1000 - 1),
				.len = (initrd_end - initrd_start + 0x1000 - 1) & ~(0x1000 - 1)
			};
			initrd = to_virt<void>(initrd_start);
			initrd_size = initrd_end - initrd_start;
		}
	}

	if (auto res_mem_opt = root.find_child("reserved-memory")) {
		auto res_mem = res_mem_opt.value();
		dtb::SizeAddrCells cells = dtb.size_cells;
		if (auto p = res_mem.prop("#size-cells")) {
			cells.size_cells = p.value().read<u32>(0);
		}
		if (auto p = res_mem.prop("#address-cells")) {
			cells.addr_cells = p.value().read<u32>(0);
		}

		for (auto child_opt = res_mem.child(); child_opt; child_opt = child_opt.value().next()) {
			auto child = child_opt.value();
			u32 index = 0;
			while (true) {
				if (auto reg_opt = child.reg(cells, index++)) {
					auto reg = reg_opt.value();
					mem_reserve[mem_reserve_count++] = {
						.base = reg.addr & ~(0x1000 - 1),
						.len = (reg.size + 0x1000 - 1) & ~(0x1000 - 1)
					};
				}
				else {
					break;
				}
			}
		}
	}

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
			aarch64_mem_check_add_usable_range(reg.addr, reg.size, mem_reserve, mem_reserve_count);
			max_addr = kstd::max(max_addr, reg.addr + reg.size);
		}
		else {
			break;
		}
	}

	auto kernel_vm_start = ALIGNUP(HHDM_START + max_addr, 1024ULL * 1024ULL * 1024ULL);
	KERNEL_VSPACE.init(kernel_vm_start, reinterpret_cast<usize>(KERNEL_START) - kernel_vm_start);

	KERNEL_PROCESS.initialize("kernel", false, EmptyHandle {}, EmptyHandle {}, EmptyHandle {});
	KERNEL_PROCESS->page_map = PageMap {AARCH64_EARLY_KERNEL_MAP->level0};
	KERNEL_PROCESS->page_map.fill_high_half();

#ifdef BOARD_MSM
	BOOT_FB.initialize(Framebuffer {
		0xA0000000,
		1080,
		2400 - 48,
		1080 * 4,
		32
	});
#endif
	LOG.lock()->register_sink(&BOOT_SINK);

	println("Total memory: ", pmalloc_get_total_mem(), " MB: ", pmalloc_get_total_mem() / 1024 / 1024);

	kernel_dtb_init(dtb);

	println("dtb device discovery start");
	dtb_discover_devices(dtb);
	println("dtb device discovery end");

	aarch64_smp_init(dtb);

	println("arch start end");
	kmain(initrd, initrd_size);
}
