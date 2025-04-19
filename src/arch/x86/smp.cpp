#include "smp.hpp"
#include "acpi/acpi.hpp"
#include "acpi/sleep.hpp"
#include "arch/cpu.hpp"
#include "arch/irq.hpp"
#include "arch/paging.hpp"
#include "arch/sleep.hpp"
#include "arch/x86/dev/hpet.hpp"
#include "arch/x86/interrupts/idt.hpp"
#include "config.hpp"
#include "cpu.hpp"
#include "cpuid.hpp"
#include "dev/dev.hpp"
#include "dev/pci.hpp"
#include "interrupts/gdt.hpp"
#include "loader/limine.h"
#include "mem/mem.hpp"
#include "qacpi/event.hpp"
#include "sched/process.hpp"
#include "sched/sched.hpp"

static volatile limine_smp_request SMP_REQUEST {
	.id = LIMINE_SMP_REQUEST,
	.revision = 0,
	.response = nullptr,
	.flags = 0
};

static Cpu* CPUS[CONFIG_MAX_CPUS] {};
static kstd::atomic<u32> NUM_CPUS {};
static Spinlock<void> SMP_LOCK {};

extern "C" void x86_syscall_stub();

static constexpr u64 USER_GDT_BASE = 0x18;
static constexpr u64 KERNEL_CS = 0x8;

static void init_usermode() {
	// enable syscall
	auto efer = msrs::IA32_EFER.read();
	efer |= 1;
	msrs::IA32_EFER.write(efer);

	msrs::IA32_LSTAR.write(reinterpret_cast<u64>(x86_syscall_stub));
	// disable interrupts on syscall entry
	msrs::IA32_SFMASK.write(0x200);
	u64 star = USER_GDT_BASE << 48 | KERNEL_CS << 32;
	msrs::IA32_STAR.write(star);
}

static void x86_detect_cpu_features() {
	Cpuid info = cpuid(1, 0);

	if (info.ecx & 1U << 30) {
		CPU_FEATURES.rdrnd = true;
	}

	if (info.ecx & 1U << 5) {
		CPU_FEATURES.vmx = true;
	}

	if (info.ecx & 1U << 26) {
		CPU_FEATURES.xsave = true;

		if (info.ecx & 1U << 28) {
			CPU_FEATURES.avx = true;
		}

		info = cpuid(7, 0);
		if (info.ebx & 1U << 16) {
			CPU_FEATURES.avx512 = true;
		}

		CPU_FEATURES.xsave_area_size = cpuid(0xD, 0).ecx;
	}
	else {
		info = cpuid(7, 0);
	}
	if (info.ecx & 1U << 2) {
		CPU_FEATURES.umip = true;
	}
	if (info.ebx & 1U << 7) {
		CPU_FEATURES.smep = true;
	}
	if (info.ebx & 1U << 20) {
		CPU_FEATURES.smap = true;
	}
	if (info.ebx & 1U << 18) {
		CPU_FEATURES.rdseed = true;
	}
}

static void x86_init_simd() {
	u64 cr0;
	asm volatile("mov %%cr0, %0" : "=r"(cr0));
	// clear EM
	cr0 &= ~(1U << 2);
	// set MP
	cr0 |= 1U << 1;
	asm volatile("mov %0, %%cr0" : : "r"(cr0));

	u64 cr4;
	asm volatile("mov %%cr4, %0" : "=r"(cr4));
	// set OSXMMEXCPT and OSFXSR
	cr4 |= 1U << 10 | 1U << 9;
	if (CPU_FEATURES.xsave) {
		// set OSXSAVE
		cr4 |= 1U << 18;
	}
	asm volatile("mov %0, %%cr4" : : "r"(cr4));

	if (CPU_FEATURES.xsave) {
		// x87 and SSE
		u64 xcr0 = 1U << 0 | 1U << 1;

		if (CPU_FEATURES.avx) {
			xcr0 |= 1U << 2;
		}

		if (CPU_FEATURES.avx512) {
			xcr0 |= 1U << 5;
			xcr0 |= 1U << 6;
			xcr0 |= 1U << 7;
		}

		wrxcr(0, xcr0);
	}
}

static void x86_cpu_resume(Cpu* self, Thread* current_thread, bool initial) {
	lapic_init(self, initial);

	u64 start = rdtsc();
	mdelay(10);
	u64 end = rdtsc();
	self->tsc_freq = (end - start) * 100;

	asm volatile("mov $6 * 8, %%ax; ltr %%ax" : : : "ax");

	self->scheduler.current = current_thread;
	set_current_thread(current_thread);
	msrs::IA32_KERNEL_GSBASE.write(current_thread->gs_base);
	msrs::IA32_FSBASE.write(current_thread->fs_base);
	init_usermode();
	x86_init_simd();

	u64 cr4;
	asm volatile("mov %%cr4, %0" : "=r"(cr4));
	if (CPU_FEATURES.umip) {
		cr4 |= 1U << 11;
	}
	if (CPU_FEATURES.smep) {
		cr4 |= 1U << 20;
	}
	if (CPU_FEATURES.smap) {
		cr4 |= 1U << 21;
	}
	asm volatile("mov %0, %%cr4" : : "r"(cr4));
}

using CtorFn = void (*)();
extern CtorFn CPU_LOCAL_CTORS_START[];
extern CtorFn CPU_LOCAL_CTORS_END[];

static void x86_init_cpu_common(Cpu* self, u8 lapic_id, bool bsp) {
	self->number = NUM_CPUS.fetch_add(1, kstd::memory_order::relaxed);
	self->lapic_id = lapic_id;

	x86_load_gdt(&self->tss);
	x86_load_idt();

	auto* thread = new Thread {"kernel main", self, &*KERNEL_PROCESS};
	thread->status = Thread::Status::Running;
	thread->pin_level = true;
	thread->pin_cpu = true;

	self->kernel_main = thread;

	self->tss.iopb = sizeof(Tss);
	x86_cpu_resume(self, thread, true);
	sched_init(bsp);

	for (auto* ctor = CPU_LOCAL_CTORS_START; ctor != CPU_LOCAL_CTORS_END; ++ctor) {
		(*ctor)();
	}
}

[[noreturn]] static void smp_ap_entry(limine_smp_info* info) {
	auto& KERNEL_MAP = KERNEL_PROCESS->page_map;
	KERNEL_MAP.use();
	Cpu* cpu = reinterpret_cast<Cpu*>(info->extra_argument);

	{
		auto guard = SMP_LOCK.lock();

		x86_init_cpu_common(cpu, info->lapic_id, false);

		// result is ignored because it doesn't need to be unregistered
		(void) cpu->cpu_tick_source->callback_producer.add_callback([cpu]() {
			cpu->scheduler.on_timer(cpu);
		});
	}

	cpu->cpu_tick_source->oneshot(50 * US_IN_MS);
	cpu->scheduler.block();
	panic("scheduler block returned");
}

extern char __CPU_LOCAL_START[];
extern char __CPU_LOCAL_END[];

void x86_smp_init() {
	lapic_first_init();
	x86_init_idt();
	x86_detect_cpu_features();

	usize cpu_local_size = __CPU_LOCAL_END - __CPU_LOCAL_START;
	auto storage = ALLOCATOR.alloc(sizeof(Cpu) + cpu_local_size);
	CPUS[0] = new (storage) Cpu {};

	x86_init_cpu_common(&*CPUS[0], SMP_REQUEST.response->bsp_lapic_id, true);

	u32 prev = 0;

	for (u64 i = 0; i < kstd::min(SMP_REQUEST.response->cpu_count, static_cast<u64>(CONFIG_MAX_CPUS)); ++i) {
		auto* cpu = SMP_REQUEST.response->cpus[i];
		if (cpu->lapic_id == SMP_REQUEST.response->bsp_lapic_id) {
			assert(i == 0);
			prev = 1;
			NUM_CPUS.store(1, kstd::memory_order::relaxed);
			continue;
		}

		storage = ALLOCATOR.alloc(sizeof(Cpu) + cpu_local_size);
		CPUS[i] = new (storage) Cpu {};

		auto ap_stack_phys = pmalloc(8);
		assert(ap_stack_phys);
		CPUS[i]->kernel_stack_base = reinterpret_cast<u64>(to_virt<void>(ap_stack_phys));

		cpu->extra_argument = reinterpret_cast<uint64_t>(&*CPUS[i]);
		cpu->goto_address = smp_ap_entry;

		while (NUM_CPUS.load(kstd::memory_order::relaxed) != prev + 1);
		prev += 1;
	}

	println("[kernel][smp]: ap init done");

	{
		auto guard = SMP_LOCK.lock();

		// result is ignored because it doesn't need to be unregistered
		(void) CPUS[0]->cpu_tick_source->callback_producer.add_callback([]() {
			CPUS[0]->scheduler.on_timer(&*CPUS[0]);
		});
	}

	CPUS[0]->cpu_tick_source->oneshot(50 * US_IN_MS);
	println("[kernel][smp]: bsp init done");
}

usize arch_get_cpu_count() {
	return NUM_CPUS.load(kstd::memory_order::relaxed);
}

Cpu* arch_get_cpu(usize index) {
	return &*CPUS[index];
}

CpuFeatures CPU_FEATURES {};

struct [[gnu::packed]] BootInfo {
	u64 stack_ptr;
	u64 entry;
	u64 arg;
	u32 page_map;
};

extern char smp_trampoline_start[];
extern char smp_trampoline_start32[];
extern char smp_trampoline_end[];

void arch_sleep_init() {
	assert(acpi::SMP_TRAMPOLINE_PHYS_ADDR);

	usize size = smp_trampoline_end - smp_trampoline_start;
	u32 magic = 0xCAFEBABE;
	for (usize i = 0; i < size; ++i) {
		if (memcmp(smp_trampoline_start + i, &magic, 4) == 0) {
			memcpy(smp_trampoline_start + i, &acpi::SMP_TRAMPOLINE_PHYS_ADDR, 4);
			break;
		}
	}

	auto virt = to_virt<void>(acpi::SMP_TRAMPOLINE_PHYS_ADDR);
	memcpy(virt, smp_trampoline_start, size);
	acpi::SMP_TRAMPLINE_PHYS_ENTRY16 = acpi::SMP_TRAMPOLINE_PHYS_ADDR;
	acpi::SMP_TRAMPLINE_PHYS_ENTRY32 = acpi::SMP_TRAMPOLINE_PHYS_ADDR + (smp_trampoline_start32 - smp_trampoline_start);
}

namespace acpi {
	extern ManuallyDestroy<qacpi::events::Context> EVENT_CTX;
}

static void ap_wake_entry(void* arg);

static void start_ap(Cpu* cpu) {
	auto* info = to_virt<BootInfo>(
		acpi::SMP_TRAMPOLINE_PHYS_ADDR +
		(smp_trampoline_end - smp_trampoline_start) -
		sizeof(BootInfo));
	info->stack_ptr = cpu->kernel_stack_base + 0x8000 - 8;
	info->entry = reinterpret_cast<u64>(ap_wake_entry);
	info->arg = reinterpret_cast<u64>(cpu);
	info->page_map = KERNEL_PROCESS->page_map.get_top_level_phys();

	asm volatile("" : : : "memory");

	lapic_boot_ap(cpu->lapic_id, acpi::SMP_TRAMPOLINE_PHYS_ADDR);
}

namespace {
	u64 MTRR_DEF_TYPE = 0;

	struct VariableMttr {
		u64 base;
		u64 mask;
	};

	VariableMttr VARIABLE_MTTRS[0x100];
	u64 FIXED_MTTRS[11];

	constexpr Msr FIXED_MTRR_MSRS[] {
		Msr {0x250},
		Msr {0x258},
		Msr {0x259},
		Msr {0x268},
		Msr {0x269},
		Msr {0x26A},
		Msr {0x26B},
		Msr {0x26C},
		Msr {0x26D},
		Msr {0x26E},
		Msr {0x26F}
	};
}

static void restore_mtrrs() {
	auto cpu_info = cpuid(1, 0);
	if (cpu_info.edx & 1 << 12) {
		auto cap = msrs::IA32_MTRRCAP.read();
		bool fixed_size_available = cap & 1 << 8;
		u8 variable_size_count = cap & 0xFF;

		for (u32 i = 0; i < variable_size_count; ++i) {
			Msr base {0x200U + i * 2};
			Msr mask {0x201U + i * 2};

			base.write(VARIABLE_MTTRS[i].base);
			mask.write(VARIABLE_MTTRS[i].mask);
		}

		if (fixed_size_available) {
			for (u32 i = 0; i < 11; ++i) {
				FIXED_MTRR_MSRS[i].write(FIXED_MTTRS[i]);
			}
		}

		msrs::IA32_MTRRDEFTYPE.write(MTRR_DEF_TYPE);
	}
}

static void ap_wake_entry(void* arg) {
	auto* cpu = static_cast<Cpu*>(arg);

	{
		auto guard = SMP_LOCK.lock();
		x86_load_gdt(&cpu->tss);
		x86_load_idt();
		restore_mtrrs();

		println("[kernel][smp]: resuming ap ", cpu->number, " after wake from sleep");

		if (cpu->saved_halt_rip) {
			x86_cpu_resume(cpu, cpu->scheduler.current, false);
		}
		else {
			x86_cpu_resume(cpu, cpu->kernel_main, false);
		}
	}

	println("[kernel][smp]: ap resume done");

	cpu->cpu_tick_source->oneshot(50 * US_IN_MS);
	if (cpu->saved_halt_rip) {
		asm volatile("mov %0, %%rsp; jmp *%1" : : "r"(cpu->saved_halt_rsp), "r"(cpu->saved_halt_rip));
	}
	else {
		cpu->scheduler.block();
	}
	panic("resumed in ap wake entry");
}

static void bsp_wake_entry(void*) {
	x86_load_gdt(&CPUS[0]->tss);
	x86_load_idt();
	restore_mtrrs();

	auto status = acpi::EVENT_CTX->prepare_for_wake();
	assert(status == qacpi::Status::Success);
	status = acpi::EVENT_CTX->wake_from_state(*acpi::GLOBAL_CTX, qacpi::events::SleepState::S3);
	assert(status == qacpi::Status::Success);

	println("[kernel][smp]: resuming after wake from sleep");

	hpet_resume();

	if (CPUS[0]->saved_halt_rip) {
		x86_cpu_resume(&*CPUS[0], CPUS[0]->scheduler.current, false);
	}
	else {
		x86_cpu_resume(&*CPUS[0], CPUS[0]->kernel_main, false);
	}

	acpi::wake_from_sleep();

	pci::resume_from_suspend();
	dev_resume_all();

	println("[kernel][smp]: bsp resume done");

	for (usize i = 1; i < NUM_CPUS.load(kstd::memory_order::relaxed); ++i) {
		auto cpu = &*CPUS[i];
		start_ap(cpu);
	}

	IrqGuard irq_guard {};
	CPUS[0]->cpu_tick_source->oneshot(50 * US_IN_MS);
	CPUS[0]->scheduler.current_irq_period = 50 * US_IN_MS;
	CPUS[0]->scheduler.us_to_next_schedule = 0;

	if (CPUS[0]->saved_halt_rip) {
		asm volatile("mov %0, %%rsp; jmp *%1" : : "r"(CPUS[0]->saved_halt_rsp), "r"(CPUS[0]->saved_halt_rip));
	}
	else {
		CPUS[0]->scheduler.block();
	}
	panic("resumed in bsp wake entry");
}

namespace {
	u32 WBINVD = 1 << 0;
	u32 WBINVD_FLUSH = 1 << 1;
}

extern char BSP_STACK[];

void arch_prepare_for_sleep() {
	auto cpu_info = cpuid(1, 0);
	if (cpu_info.edx & 1 << 12) {
		auto cap = msrs::IA32_MTRRCAP.read();
		bool fixed_size_available = cap & 1 << 8;
		u8 variable_size_count = cap & 0xFF;

		MTRR_DEF_TYPE = msrs::IA32_MTRRDEFTYPE.read();

		for (u32 i = 0; i < variable_size_count; ++i) {
			Msr base {0x200U + i * 2};
			Msr mask {0x201U + i * 2};
			VARIABLE_MTTRS[i].base = base.read();
			VARIABLE_MTTRS[i].mask = mask.read();
		}

		if (fixed_size_available) {
			for (u32 i = 0; i < 11; ++i) {
				FIXED_MTTRS[i] = FIXED_MTRR_MSRS[i].read();
			}
		}
	}

	auto* info = to_virt<BootInfo>(
		acpi::SMP_TRAMPOLINE_PHYS_ADDR +
		(smp_trampoline_end - smp_trampoline_start) -
		sizeof(BootInfo));

	info->stack_ptr = reinterpret_cast<u64>(BSP_STACK) + 0x8000 - 8;
	info->entry = reinterpret_cast<u64>(bsp_wake_entry);
	info->page_map = KERNEL_PROCESS->page_map.get_top_level_phys();
	assert(info->page_map <= 0xFFFFFFFF);

	IrqGuard irq_guard {};
	auto cpu = get_current_thread()->cpu;

	for (usize i = 0; i < arch_get_cpu_count(); ++i) {
		if (i == cpu->number) {
			continue;
		}

		auto* other = arch_get_cpu(i);
		arch_send_ipi(Ipi::Halt, other);
		other->ipi_ack.store(false, kstd::memory_order::seq_cst);
		while (!other->ipi_ack.load(kstd::memory_order::seq_cst));
	}

	if ((acpi::GLOBAL_FADT->flags & WBINVD_FLUSH) ||
		(acpi::GLOBAL_FADT->flags & WBINVD)) {
		asm volatile("wbinvd");
	}
}
