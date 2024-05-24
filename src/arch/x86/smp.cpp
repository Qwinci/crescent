#include "smp.hpp"
#include "arch/cpu.hpp"
#include "arch/paging.hpp"
#include "arch/x86/interrupts/idt.hpp"
#include "config.hpp"
#include "cpu.hpp"
#include "cpuid.hpp"
#include "interrupts/gdt.hpp"
#include "loader/limine.h"
#include "sched/process.hpp"
#include "sched/sched.hpp"
#include "sys/syscalls.hpp"

static volatile limine_smp_request SMP_REQUEST {
	.id = LIMINE_SMP_REQUEST,
	.revision = 0,
	.response = nullptr,
	.flags = 0
};

static ManuallyInit<Cpu> CPUS[CONFIG_MAX_CPUS] {};
static kstd::atomic<int> NUM_CPUS {};
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

static void x86_init_cpu_common(Cpu* self, u8 lapic_id) {
	self->number = NUM_CPUS.fetch_add(1, kstd::memory_order::relaxed);
	self->lapic_id = lapic_id;
	println("[kernel][smp]: cpu ", self->number, " online!");
	x86_load_gdt(&self->tss);
	x86_load_idt();
	lapic_init(self);

	self->tss.iopb = sizeof(Tss);
	asm volatile("mov $6 * 8, %%ax; ltr %%ax" : : : "ax");

	auto* thread = new Thread {"kernel main", self, &*KERNEL_PROCESS};
	thread->status = Thread::Status::Running;
	self->scheduler.current = thread;
	set_current_thread(thread);
	init_usermode();
	sched_init();

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

	println("[kernel][smp]: cpu init done");
}

[[noreturn]] static void smp_ap_entry(limine_smp_info* info) {
	auto& KERNEL_MAP = KERNEL_PROCESS->page_map;
	KERNEL_MAP.use();
	Cpu* cpu = reinterpret_cast<Cpu*>(info->extra_argument);

	{
		auto guard = SMP_LOCK.lock();

		x86_init_cpu_common(cpu, info->lapic_id);

		// result is ignored because it doesn't need to be unregistered
		(void) cpu->cpu_tick_source->callback_producer.add_callback([cpu]() {
			cpu->scheduler.on_timer(cpu);
		});
	}

	cpu->cpu_tick_source->oneshot(50 * US_IN_MS);
	cpu->scheduler.block();
	panic("scheduler block returned");
}

void x86_smp_init() {
	lapic_first_init();
	x86_init_idt();
	x86_detect_cpu_features();

	CPUS[0].initialize();
	x86_init_cpu_common(&*CPUS[0], SMP_REQUEST.response->bsp_lapic_id);

	int prev = 0;

	for (u64 i = 0; i < kstd::min(SMP_REQUEST.response->cpu_count, static_cast<u64>(CONFIG_MAX_CPUS)); ++i) {
		auto* cpu = SMP_REQUEST.response->cpus[i];
		if (cpu->lapic_id == SMP_REQUEST.response->bsp_lapic_id) {
			assert(i == 0);
			prev = 1;
			NUM_CPUS.store(1, kstd::memory_order::relaxed);
			continue;
		}
		CPUS[i].initialize();
		cpu->extra_argument = reinterpret_cast<uint64_t>(&*CPUS[i]);
		cpu->goto_address = smp_ap_entry;

		while (NUM_CPUS.load(kstd::memory_order::relaxed) != prev + 1);
		prev += 1;
	}

	println("[kernel][smp]: init done");

	{
		auto guard = SMP_LOCK.lock();
		lapic_init_finalize();

		// result is ignored because it doesn't need to be unregistered
		(void) CPUS[0]->cpu_tick_source->callback_producer.add_callback([]() {
			CPUS[0]->scheduler.on_timer(&*CPUS[0]);
		});
	}

	CPUS[0]->cpu_tick_source->oneshot(50 * US_IN_MS);
}

CpuFeatures CPU_FEATURES {};
