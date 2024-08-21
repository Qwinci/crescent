#include "smp.hpp"
#include "arch/cpu.hpp"
#include "config.hpp"
#include "dev/psci.hpp"
#include "loader/constants.hpp"
#include "loader/early_paging.hpp"
#include "mem/mem.hpp"
#include "sched/process.hpp"
#include "dev/gic.hpp"

static ManuallyInit<Cpu> CPUS[CONFIG_MAX_CPUS] {};
static kstd::atomic<u32> NUM_CPUS {1};
static Spinlock<void> SMP_LOCK {};

asm(R"(
.pushsection .text
.globl aarch64_ap_entry_asm
aarch64_ap_entry_asm:
	mov sp, x0

	mov x0, #0
	orr x0, x0, #(1 << 29)
	orr x0, x0, #(1 << 28)
	orr x0, x0, #(1 << 23)
	orr x0, x0, #(1 << 22)
	orr x0, x0, #(1 << 20)
	orr x0, x0, #(1 << 11)
	orr x0, x0, #(1 << 12)
	orr x0, x0, #(1 << 2)
	msr sctlr_el1, x0

	mov x0, #0x3C5
	msr spsr_el1, x0

	adrp x0, aarch64_ap_entry_stage0
	add x0, x0, :lo12:aarch64_ap_entry_stage0
	msr elr_el1, x0

	mov x29, #0
	mov x30, #0
	eret
.popsection
)");

extern EarlyPageMap* AARCH64_IDENTITY_MAP;
[[gnu::visibility("hidden")]] extern char KERNEL_START[];

extern "C" [[noreturn, gnu::used]] void aarch64_ap_entry_stage0() {
	u64 ttbr0_el1 = reinterpret_cast<u64>(AARCH64_IDENTITY_MAP);
	u64 ttbr1_el1 = to_phys(KERNEL_PROCESS->page_map.level0);
	asm volatile("msr ttbr0_el1, %0" : : "r"(ttbr0_el1));
	asm volatile("msr ttbr1_el1, %0" : : "r"(ttbr1_el1));

	u64 id_aa64mmfr0_el1;
	asm volatile("mrs %0, id_aa64mmfr0_el1" : "=r"(id_aa64mmfr0_el1));

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

	asm volatile("msr tcr_el1, %0" : : "r"(tcr_el1));

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
	asm volatile("msr mair_el1, %0" : : "r"(mair_el1));

	// force changes
	asm volatile("isb");

	u64 sctlr_el1;
	asm volatile("mrs %0, sctlr_el1" : "=r"(sctlr_el1));
	// enable mmu
	sctlr_el1 |= 1;
	// enable cache
	sctlr_el1 |= 1 << 2;
	sctlr_el1 |= 1 << 12;
	sctlr_el1 &= ~(1 << 19);
	asm volatile("tlbi vmalle1; dsb nsh; msr sctlr_el1, %0; isb" : : "r"(sctlr_el1) : "memory");

	asm volatile(R"(
		add sp, sp, %2
		adrp x2, aarch64_ap_entry
		add x2, x2, :lo12:aarch64_ap_entry
		sub x2, x2, %0
		add x2, x2, %1
		br x2
		)" : : "r"(KERNEL_START), "r"(KERNEL_OFFSET), "r"(HHDM_OFFSET) : "x2");
	__builtin_unreachable();
}

static void aarch64_common_cpu_init(Cpu* self, u32 num, bool bsp) {
	self->number = num;

	auto* thread = new Thread {"kernel main", self, &*KERNEL_PROCESS};
	thread->status = Thread::Status::Running;
	self->scheduler.current = thread;
	set_current_thread(thread);

	// enable simd
	u64 cpacr_el1;
	asm volatile("mrs %0, cpacr_el1" : "=r"(cpacr_el1));
	cpacr_el1 |= 0b11 << 20;
	asm volatile("msr cpacr_el1, %0" : : "r"(cpacr_el1));

	sched_init(bsp);

	u64 mpidr;
	asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
	self->affinity = (mpidr & 0xFFFFFF) | (mpidr >> 32 & 0xFF) << 24;
}

extern char EXCEPTION_HANDLERS_START[];

extern "C" [[noreturn, gnu::used]] void aarch64_ap_entry() {
	asm volatile("msr vbar_el1, %0" : : "r"(EXCEPTION_HANDLERS_START));

	Cpu* cpu;

	{
		u32 num = NUM_CPUS.load(kstd::memory_order::relaxed);

		auto lock = SMP_LOCK.lock();
		CPUS[num].initialize();
		cpu = &*CPUS[num];
		aarch64_common_cpu_init(cpu, num, false);
		gic_init_on_cpu();
		cpu->arm_tick_source.init_on_cpu(cpu);

		println("[kernel][smp]: cpu ", num, " online!");

		NUM_CPUS.store(num + 1, kstd::memory_order::relaxed);
		asm volatile("sev");
	}

	cpu->cpu_tick_source->oneshot(50 * US_IN_MS);
	cpu->scheduler.block();
	panic("scheduler block returned");
}

extern "C" void aarch64_ap_entry_asm();

void aarch64_bsp_init() {
	CPUS[0].initialize();
	aarch64_common_cpu_init(&*CPUS[0], 0, true);
}

void aarch64_smp_init(dtb::Dtb& dtb) {
	CPUS[0]->arm_tick_source.init_on_cpu(&*CPUS[0]);

	auto root = dtb.get_root();

	auto cpus_opt = root.find_child("cpus");
	if (!cpus_opt) {
		return;
	}
	auto cpus = cpus_opt.value();

	u64 self_mpidr;
	asm volatile ("mrs %0, mpidr_el1" : "=r"(self_mpidr));
	u64 self_aff = (self_mpidr & 0xFFFFFF) | (self_mpidr >> 32 & 0xFF) << 32;

	auto cells = cpus.size_cells();
	for (auto child : cpus.child_iter()) {
		if (NUM_CPUS.load(kstd::memory_order::relaxed) >= CONFIG_MAX_CPUS) {
			break;
		}

		if (child.name != "cpu") {
			continue;
		}
		auto enable_method_opt = child.prop("enable-method");
		if (!enable_method_opt) {
			continue;
		}
		kstd::string_view enable_method {static_cast<const char*>(enable_method_opt->ptr)};
		if (enable_method != "psci") {
			continue;
		}
		auto mpidr = child.reg(cells, 0).value().addr;
		if (mpidr == self_aff) {
			continue;
		}

		auto& KERNEL_MAP = KERNEL_PROCESS->page_map;

		auto phys_entry = KERNEL_MAP.get_phys(reinterpret_cast<u64>(aarch64_ap_entry_asm));
		assert(phys_entry);

		auto stack_phys = pmalloc(16);
		assert(stack_phys);
		u8* stack = to_virt<u8>(stack_phys);
		stack += 16 * PAGE_SIZE;

		auto old = NUM_CPUS.load(kstd::memory_order::relaxed);

		psci_cpu_on(mpidr, phys_entry, to_phys(stack));

		while (NUM_CPUS.load(kstd::memory_order::relaxed) != old + 1) {
			asm volatile("wfe");
		}
	}

	println("[kernel][aarch64]: smp init done");

	// result is ignored because it doesn't need to be unregistered
	(void) CPUS[0]->cpu_tick_source->callback_producer.add_callback([]() {
		CPUS[0]->scheduler.on_timer(&*CPUS[0]);
	});

	CPUS[0]->cpu_tick_source->oneshot(50 * US_IN_MS);
}

usize arch_get_cpu_count() {
	return NUM_CPUS.load(kstd::memory_order::relaxed);
}

Cpu* arch_get_cpu(usize index) {
	return &*CPUS[index];
}
