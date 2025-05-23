#include "dev/discovery.hpp"
#include "loader/early_paging.hpp"
#include "mem/aarch64_mem.hpp"
#include "dtb.hpp"
#include "mem/mem.hpp"
#include "mem/pmalloc.hpp"
#include "mem/vspace.hpp"
#include "sched/process.hpp"
#include "smp.hpp"
#include "stdio.hpp"

#include "dev/fb/fb.hpp"
#include "dev/ramfb.hpp"

volatile uint8_t* qemu_uart = reinterpret_cast<uint8_t*>(0xFFFF000009000000);
void qemu_uart_puts(kstd::string_view str) {
	for (auto c : str) {
		*qemu_uart = c;
	}
}

void qemu_uart_puts(usize num) {
	char buf[20];
	char* ptr = buf + 20;
	do {
		*--ptr = '0' + (num % 10);
		num /= 10;
	} while (num);
	qemu_uart_puts(kstd::string_view {ptr, static_cast<size_t>(buf + 20 - ptr)});
}

void uart_hex(usize num) {
	char buf[16];
	char* ptr = buf + 16;
	static constexpr char HEX_CHARS[] = "0123456789ABCDEF";
	do {
		*--ptr = HEX_CHARS[num % 16];
		num /= 16;
	} while (num);
	qemu_uart_puts(kstd::string_view {ptr, static_cast<size_t>(buf + 16 - ptr)});
}

[[gnu::visibility("hidden")]] extern char KERNEL_START[];
[[gnu::visibility("hidden")]] extern char KERNEL_END[];

struct QemuUartSink : LogSink {
	void write(kstd::string_view str) override {
		qemu_uart_puts(str);
	}
};

static QemuUartSink VIRT_UART_BOOT_SINK;

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

extern void kernel_dtb_init(void* plain_dtb);

extern EarlyPageMap* AARCH64_EARLY_KERNEL_MAP;

u64 arch_get_random_seed() {
	// todo improve
	u64 count;
	asm volatile("mrs %0, cntvct_el0" : "=r"(count));
	return count;
}

extern "C" [[noreturn, gnu::used]] void arch_start(usize dtb_phys, usize kernel_phys) {
	run_constructors();

	auto dtb_ptr = to_virt<void>(dtb_phys);

	println("arch start begin");
	asm volatile("msr vbar_el1, %0" : : "r"(EXCEPTION_HANDLERS_START));

	// bit0 == fiq, bit1 == irq, bit2 == SError, bit3 == Debug
	asm volatile("msr daifclr, #0b1111");

	//asm volatile("msr ttbr0_el1, %0" : : "r"(0ULL));
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
		.base = to_phys(dtb_ptr) & ~0xFFF,
		.len = (dtb.total_size + (to_phys(dtb_ptr) & 0xFFF) + 0xFFF) & ~0xFFF
	};

	for (auto& reserved : dtb.get_reserved()) {
		u64 addr = kstd::byteswap(reserved.addr);
		u64 size = kstd::byteswap(reserved.size);
		mem_reserve[mem_reserve_count++] = {
			.base = addr & ~0xFFF,
			.len = (size + (addr & 0xFFF) + 0xFFF) & ~0xFFF
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
				.base = initrd_start & ~0xFFF,
				.len = ((initrd_start & 0xFFF) + (initrd_end - initrd_start) + 0xFFF) & ~0xFFF
			};
			initrd = to_virt<void>(initrd_start);
			initrd_size = initrd_end - initrd_start;
		}
	}

	auto res_mem_opt = root.find_child("reserved-memory");
	dtb::SizeAddrCells res_mem_cells = dtb.size_cells;
	if (res_mem_opt) {
		auto res_mem = res_mem_opt.value();
		if (auto p = res_mem.prop("#size-cells")) {
			res_mem_cells.size_cells = p.value().read<u32>(0);
		}
		if (auto p = res_mem.prop("#address-cells")) {
			res_mem_cells.addr_cells = p.value().read<u32>(0);
		}

		for (auto child_opt = res_mem.child(); child_opt; child_opt = child_opt.value().next()) {
			auto child = child_opt.value();
			u32 index = 0;
			while (true) {
				if (auto reg_opt = child.reg(res_mem_cells, index++)) {
					auto reg = reg_opt.value();
					mem_reserve[mem_reserve_count++] = {
						.base = reg.addr & ~0xFFF,
						.len = ((reg.addr & 0xFFF) + reg.size + 0xFFF) & ~0xFFF
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

	usize fb_addr = 0;
	if (res_mem_opt) {
		for (auto& child : res_mem_opt->child_iter()) {
			if (auto label = child.prop("label")) {
				kstd::string_view str {static_cast<const char*>(label->ptr), label->len - 1};
				if (str == "cont_splash_region" || str == "cont_splash_memory") {
					if (auto reg_opt = child.reg(res_mem_cells, 0)) {
						fb_addr = reg_opt->addr;
						break;
					}
				}
			}
		}
	}

	if (fb_addr) {
		BOOT_FB.initialize(
			fb_addr,
			1080U,
			2400U - 48,
			1080U * 4,
			32U);
		BOOT_FB->row = 4;
		BOOT_FB->scale = 4;
	}

	bool is_virt = false;
	if (auto compatible_opt = dtb.get_root().compatible()) {
		usize offset = 0;
		auto compatible = compatible_opt.value();
		while (true) {
			auto end = compatible.find(' ', offset);
			if (end == kstd::string_view::npos) {
				auto str = compatible.substr(offset);
				if (str == "linux,dummy-virt") {
					is_virt = true;
				}
				break;
			}

			auto str = compatible.substr(offset, end - offset);
			if (str == "linux,dummy-virt") {
				is_virt = true;
				break;
			}
			offset = end + 1;
		}
	}

	if (is_virt) {
		LOG.lock()->register_sink(&VIRT_UART_BOOT_SINK);
	}

	println("total memory: ", pmalloc_get_total_mem(), " MB: ", pmalloc_get_total_mem() / 1024 / 1024);

	kernel_dtb_init(&dtb);
	aarch64_bsp_init();

	println("dtb device discovery start");
	dtb_discover_devices();
	println("dtb device discovery end");

	if (is_virt) {
		ramfb_init();
	}

	aarch64_smp_init(dtb);

	println("arch start end");
	kmain(initrd, initrd_size);
}
