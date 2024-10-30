#include "sched/sched.hpp"
#include "arch/cpu.hpp"
#include "sched/process.hpp"

asm(R"(
.pushsection .text
.arch armv8-a+fp-armv8
.globl sched_switch_thread
.type sched_switch_thread, @function

// void sched_switch_thread(ArchThread* prev, ArchThread* next)
sched_switch_thread:
	sub sp, sp, #128
	stp x0,  x1,  [sp, #0]
	stp x19, x20, [sp, #16]
	stp x21, x22, [sp, #32]
	stp x23, x24, [sp, #48]
	stp x25, x26, [sp, #64]
	stp x27, x28, [sp, #80]
	stp x29, x30, [sp, #96]
	mrs x2, daif
	str x2, [sp, #112]

	mov x2, sp
	str x2, [x0]

	// prev->sched_lock = false
	strb wzr, [x0, #208]

	ldr x3, [x1]
	mov sp, x3

	ldp x0,  x1,  [sp, #0]
	ldp x19, x20, [sp, #16]
	ldp x21, x22, [sp, #32]
	ldp x23, x24, [sp, #48]
	ldp x25, x26, [sp, #64]
	ldp x27, x28, [sp, #80]
	ldp x29, x30, [sp, #96]
	ldr x2, [sp, #112]
	msr daif, x2
	add sp, sp, #128
	ret
.popsection
)");

[[noreturn]] void arch_idle_fn(void*) {
	auto& scheduler = get_current_thread()->cpu->scheduler;
	while (true) {
		{
			IrqGuard irq_guard {};
			for (auto& level : scheduler.levels) {
				if (!level.list.lock()->is_empty()) {
					scheduler.update_schedule();
					scheduler.enable_preemption(scheduler.current->cpu);
					scheduler.do_schedule();
				}
			}
		}
		asm volatile("wfi");
	}
}

Thread* get_current_thread() {
	Thread* thread;
	asm volatile("mrs %0, tpidr_el1" : "=r"(thread));
	return thread;
}

void set_current_thread(Thread* thread) {
	asm volatile("msr tpidr_el1, %0" : : "r"(thread));
}

asm(R"(
.pushsection .text
.globl save_simd_regs
.type save_simd_regs, @function
save_simd_regs:
	stp q0, q1, [x0, #0]
	stp q2, q3, [x0, #32]
	stp q4, q5, [x0, #64]
	stp q6, q7, [x0, #96]
	stp q8, q9, [x0, #128]
	stp q10, q11, [x0, #160]
	stp q12, q13, [x0, #192]
	stp q14, q15, [x0, #224]
	stp q16, q17, [x0, #256]
	stp q18, q19, [x0, #288]
	stp q20, q21, [x0, #320]
	stp q22, q23, [x0, #352]
	stp q24, q25, [x0, #384]
	stp q26, q27, [x0, #416]
	stp q28, q29, [x0, #448]
	stp q30, q31, [x0, #480]
	mrs x1, fpcr
	mrs x2, fpsr
	add x0, x0, #512
	stp x1, x2, [x0]
	ret

restore_simd_regs:
	ldp q0, q1, [x0, #0]
	ldp q2, q3, [x0, #32]
	ldp q4, q5, [x0, #64]
	ldp q6, q7, [x0, #96]
	ldp q8, q9, [x0, #128]
	ldp q10, q11, [x0, #160]
	ldp q12, q13, [x0, #192]
	ldp q14, q15, [x0, #224]
	ldp q16, q17, [x0, #256]
	ldp q18, q19, [x0, #288]
	ldp q20, q21, [x0, #320]
	ldp q22, q23, [x0, #352]
	ldp q24, q25, [x0, #384]
	ldp q26, q27, [x0, #416]
	ldp q28, q29, [x0, #448]
	ldp q30, q31, [x0, #480]
	add x0, x0, #512
	ldp x1, x2, [x0]
	msr fpcr, x1
	msr fpsr, x2
	ret

.popsection
)");

extern "C" void save_simd_regs(u8* regs);
extern "C" void restore_simd_regs(u8* regs);

void sched_before_switch(Thread* prev, Thread* thread) {
	if (prev->process->user) {
		save_simd_regs(prev->simd);
	}

	if (thread->process->user) {
		restore_simd_regs(thread->simd);
		asm volatile("msr tpidr_el0, %0" : : "r"(thread->tpidr_el0));
	}
}

constexpr usize KERNEL_STACK_SIZE = 1024 * 32;
constexpr usize USER_STACK_SIZE = 1024 * 1024;

struct InitFrame {
	u64 x[14];
	u64 daif;
	u64 pad;
};

extern "C" void arch_on_first_switch();
extern "C" void arch_on_first_switch_user();

asm(R"(
.pushsection .text
.globl arch_on_first_switch
arch_on_first_switch:
	msr daifclr, #0b10
	br x1

.globl arch_on_first_switch_user
arch_on_first_switch_user:
	// x1 == user ip
	msr elr_el1, x1
	// x19 == user sp
	msr sp_el0, x19

	mov x1, #0
	msr spsr_el1, x1
	mov x2, #0
	mov x3, #0
	mov x4, #0
	mov x5, #0
	mov x6, #0
	mov x7, #0
	mov x8, #0
	mov x9, #0
	mov x10, #0
	mov x11, #0
	mov x12, #0
	mov x13, #0
	mov x14, #0
	mov x15, #0
	mov x16, #0
	mov x17, #0
	mov x18, #0
	mov x19, #0

	eret
.popsection
)");

struct SimdRegisters {
	u64 v[64];
	u64 fpcr;
	u64 fpsr;
};
static_assert(sizeof(SimdRegisters) == 528);

ArchThread::ArchThread(void (*fn)(void* arg), void* arg, Process* process) {
	kernel_stack_base = new usize[KERNEL_STACK_SIZE / 8] {};
	syscall_sp = reinterpret_cast<u8*>(kernel_stack_base) + KERNEL_STACK_SIZE;
	sp = reinterpret_cast<u8*>(kernel_stack_base) + KERNEL_STACK_SIZE - sizeof(InitFrame);
	auto* frame = reinterpret_cast<InitFrame*>(sp);
	memset(frame, 0, sizeof(InitFrame));
	frame->x[0] = reinterpret_cast<u64>(arg);
	frame->x[1] = reinterpret_cast<u64>(fn);
	if (process->user) {
		user_stack_base = reinterpret_cast<u8*>(process->allocate(
			nullptr,
			USER_STACK_SIZE + PAGE_SIZE,
			MemoryAllocFlags::Read | MemoryAllocFlags::Write | MemoryAllocFlags::Backed, nullptr));
		assert(user_stack_base);
		process->page_map.protect(reinterpret_cast<u64>(user_stack_base), PageFlags::User | PageFlags::Read, CacheMode::WriteBack);
		simd = static_cast<u8*>(ALLOCATOR.alloc(sizeof(SimdRegisters)));
		assert(simd);
		memset(simd, 0, sizeof(SimdRegisters));
		frame->x[13] = reinterpret_cast<u64>(arch_on_first_switch_user);
		frame->x[2] = reinterpret_cast<u64>(user_stack_base + USER_STACK_SIZE + PAGE_SIZE);
	}
	else {
		frame->x[13] = reinterpret_cast<u64>(arch_on_first_switch);
	}
}

ArchThread::~ArchThread() {
	ALLOCATOR.free(kernel_stack_base, KERNEL_STACK_SIZE);
	if (user_stack_base) {
		static_cast<Thread*>(this)->process->free(reinterpret_cast<usize>(user_stack_base), USER_STACK_SIZE);
	}
	if (simd) {
		ALLOCATOR.free(simd, sizeof(SimdRegisters));
	}
}
