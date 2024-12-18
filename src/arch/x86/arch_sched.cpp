#include "arch/arch_thread.hpp"
#include "arch/cpu.hpp"
#include "assert.hpp"
#include "cpu.hpp"
#include "mem/vspace.hpp"
#include "sched/sched.hpp"
#include "simd_state.hpp"

asm(R"(
.pushsection .text
.globl sched_switch_thread
.type sched_switch_thread, @function

// void sched_switch_thread(ArchThread* prev, ArchThread* next)
sched_switch_thread:
	pushfq
	push %rdi
	push %rbx
	push %rbp
	push %r12
	push %r13
	push %r14
	push %r15

	mov %rsp, 8(%rdi)

	// prev->sched_lock = false
	movb $0, 223(%rdi)

	mov 8(%rsi), %rsp

	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %rbp
	pop %rbx
	pop %rdi
	popfq
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
					scheduler.do_schedule();
				}
			}
		}
		asm volatile("hlt");
	}
}

Thread* get_current_thread() {
	Thread* thread;
	asm volatile("movq %%gs:0, %0" : "=r"(thread));
	assert(thread->self == thread);
	return thread;
}

void sched_before_switch(Thread* prev, Thread* thread) {
	if (prev->process->user) {
		if (CPU_FEATURES.xsave) {
			xsave(prev->simd, ~0);
		}
		else {
			asm volatile("fxsaveq %0" : : "m"(*prev->simd) : "memory");
		}
	}

	if (thread->process->user) {
		if (CPU_FEATURES.xsave) {
			xrstor(thread->simd, ~0);
		}
		else {
			asm volatile("fxrstorq %0" : : "m"(*thread->simd) : "memory");
		}

		msrs::IA32_FSBASE.write(thread->fs_base);
		msrs::IA32_KERNEL_GSBASE.write(thread->gs_base);
	}

	auto& tss = thread->cpu->tss;
	auto kernel_sp = reinterpret_cast<usize>(thread->syscall_sp);
	tss.rsp0_low = kernel_sp;
	tss.rsp0_high = kernel_sp >> 32;
}

void set_current_thread(Thread* thread) {
	assert(thread->self == thread);
	msrs::IA32_GSBASE.write(reinterpret_cast<u64>(thread));
}

struct InitFrame {
	u64 r15;
	u64 r14;
	u64 r13;
	u64 r12;
	u64 rbp;
	u64 rbx;
	u64 rdi;
	u64 rflags;
	u64 on_first_switch;
	u64 rip;
};

struct UserInitFrame {
	u64 r15;
	u64 r14;
	u64 r13;
	u64 r12;
	u64 rbp;
	u64 rbx;
	u64 rdi;
	u64 kernel_rflags;
	u64 on_first_switch;
	u64 rip;
	u64 rflags;
	u64 rsp;
};

constexpr usize KERNEL_STACK_SIZE = 1024 * 64;
constexpr usize USER_STACK_SIZE = 1024 * 1024;

extern "C" void arch_on_first_switch();
extern "C" void arch_on_first_switch_user();

asm(R"(
.pushsection .text
.globl arch_on_first_switch
arch_on_first_switch:
	sti
	ret

.globl arch_on_first_switch_user
arch_on_first_switch_user:
	xor %eax, %eax
	xor %ebx, %ebx
	xor %edx, %edx
	xor %r8d, %r8d
	xor %r9d, %r9d
	xor %r10d, %r10d
	xor %r11d, %r11d
	xor %r12d, %r12d
	xor %r13d, %r13d
	xor %r14d, %r14d
	xor %r15d, %r15d

	pop %rcx
	pop %r11
	pop %rsp
	swapgs
	sysretq
.popsection
)");

ArchThread::ArchThread(void (*fn)(void* arg), void* arg, Process* process) : self {this} {
	kernel_stack_base = new usize[(KERNEL_STACK_SIZE + PAGE_SIZE) / 8] {};
	syscall_sp = reinterpret_cast<u8*>(kernel_stack_base) + KERNEL_STACK_SIZE + PAGE_SIZE;
	sp = reinterpret_cast<u8*>(kernel_stack_base) + KERNEL_STACK_SIZE + PAGE_SIZE - (process->user ? sizeof(UserInitFrame) : (sizeof(InitFrame) + 8));
	auto* frame = reinterpret_cast<InitFrame*>(sp);
	memset(frame, 0, sizeof(InitFrame));
	frame->rdi = reinterpret_cast<u64>(arg);

	KERNEL_PROCESS->page_map.protect(reinterpret_cast<u64>(kernel_stack_base), PageFlags::Read, CacheMode::WriteBack);

	frame->rip = reinterpret_cast<u64>(fn);
	frame->rflags = 2;

	if (process->user) {
		user_stack_base = reinterpret_cast<u8*>(process->allocate(
			nullptr,
			USER_STACK_SIZE + PAGE_SIZE,
			MemoryAllocFlags::Read | MemoryAllocFlags::Write | MemoryAllocFlags::Backed, nullptr));
		assert(user_stack_base);
		process->page_map.protect(reinterpret_cast<u64>(user_stack_base), PageFlags::User | PageFlags::Read, CacheMode::WriteBack);
		auto simd_size = CPU_FEATURES.xsave ? CPU_FEATURES.xsave_area_size : sizeof(FxState);
		simd = static_cast<u8*>(KERNEL_VSPACE.alloc_backed(simd_size, PageFlags::Read | PageFlags::Write));
		assert(simd);
		assert(reinterpret_cast<usize>(simd) % 64 == 0);
		auto* fx_state = reinterpret_cast<FxState*>(simd);
		memset(fx_state, 0, simd_size);
		// IM | DM | ZM | OM | UM | PM | PC
		fx_state->fcw = 1 << 0 | 1 << 1 | 1 << 2 | 1 << 3 | 1 << 4 | 1 << 5 | 0b11 << 8;
		fx_state->mxcsr = 0b1111110000000;

		frame->on_first_switch = reinterpret_cast<u64>(arch_on_first_switch_user);
		auto* user_frame = reinterpret_cast<UserInitFrame*>(frame);
		user_frame->rflags = 0x202;
		user_frame->rsp = reinterpret_cast<u64>(user_stack_base + USER_STACK_SIZE + PAGE_SIZE - 8);
	}
	else {
		frame->on_first_switch = reinterpret_cast<u64>(arch_on_first_switch);
	}
}

#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_BASE 7
#define AT_ENTRY 9

ArchThread::ArchThread(const SysvInfo& sysv, Process* process) : self {this} {
	assert(process->user);

	kernel_stack_base = new usize[(KERNEL_STACK_SIZE + PAGE_SIZE) / 8] {};
	syscall_sp = reinterpret_cast<u8*>(kernel_stack_base) + KERNEL_STACK_SIZE + PAGE_SIZE;
	sp = reinterpret_cast<u8*>(kernel_stack_base) + KERNEL_STACK_SIZE + PAGE_SIZE - (process->user ? sizeof(UserInitFrame) : (sizeof(InitFrame) + 8));
	auto* frame = reinterpret_cast<InitFrame*>(sp);
	memset(frame, 0, sizeof(InitFrame));

	KERNEL_PROCESS->page_map.protect(reinterpret_cast<u64>(kernel_stack_base), PageFlags::Read, CacheMode::WriteBack);

	frame->rflags = 2;
	frame->rip = sysv.ld_entry;

	user_stack_base = reinterpret_cast<u8*>(process->allocate(
		nullptr,
		USER_STACK_SIZE + PAGE_SIZE,
		MemoryAllocFlags::Read | MemoryAllocFlags::Write | MemoryAllocFlags::Backed, nullptr));
	assert(user_stack_base);
	process->page_map.protect(reinterpret_cast<u64>(user_stack_base), PageFlags::User | PageFlags::Read, CacheMode::WriteBack);

	auto simd_size = CPU_FEATURES.xsave ? CPU_FEATURES.xsave_area_size : sizeof(FxState);
	simd = static_cast<u8*>(KERNEL_VSPACE.alloc_backed(simd_size, PageFlags::Read | PageFlags::Write));
	assert(simd);
	assert(reinterpret_cast<usize>(simd) % 64 == 0);

	auto* fx_state = reinterpret_cast<FxState*>(simd);
	memset(fx_state, 0, simd_size);
	// IM | DM | ZM | OM | UM | PM | PC
	fx_state->fcw = 1 << 0 | 1 << 1 | 1 << 2 | 1 << 3 | 1 << 4 | 1 << 5 | 0b11 << 8;
	fx_state->mxcsr = 0b1111110000000;

	frame->on_first_switch = reinterpret_cast<u64>(arch_on_first_switch_user);
	auto* user_frame = reinterpret_cast<UserInitFrame*>(frame);
	user_frame->rflags = 0x202;

	usize data_size = 16 * 8;
	usize aligned_data_size = (data_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	auto kernel_mapping = KERNEL_VSPACE.alloc(aligned_data_size);
	assert(kernel_mapping);
	for (usize i = 0; i < aligned_data_size; i += PAGE_SIZE) {
		auto status = KERNEL_PROCESS->page_map.map(
			reinterpret_cast<u64>(kernel_mapping) + i,
			process->page_map.get_phys(reinterpret_cast<u64>(user_stack_base) + USER_STACK_SIZE + PAGE_SIZE - aligned_data_size + i),
			PageFlags::Read | PageFlags::Write,
			CacheMode::WriteBack);
		assert(status);
	}

	u64 user_rsp = offset(user_stack_base, u64, USER_STACK_SIZE + PAGE_SIZE);
	u64* kernel_user_rsp = offset(kernel_mapping, u64*, aligned_data_size);

	auto push = [&](u64 value) {
		*--kernel_user_rsp = value;
		user_rsp -= 8;
	};

	// align to 16 bytes
	push(0);

	// aux end
	push(0);
	push(0);

	push(sysv.exe_entry);
	push(AT_ENTRY);

	push(sysv.ld_base);
	push(AT_BASE);

	push(sysv.exe_phdr_count);
	push(AT_PHNUM);

	push(sysv.exe_phdr_size);
	push(AT_PHENT);

	push(sysv.exe_phdrs_addr);
	push(AT_PHDR);

	// env end
	push(0);
	// env (nothing)
	// argv end
	push(0);
	// argv (nothing)
	// argc
	push(0);

	for (usize i = 0; i < aligned_data_size; i += PAGE_SIZE) {
		KERNEL_PROCESS->page_map.unmap(reinterpret_cast<u64>(kernel_mapping) + i);
	}
	KERNEL_VSPACE.free(kernel_mapping, aligned_data_size);

	user_frame->rsp = user_rsp;
}

ArchThread::~ArchThread() {
	ALLOCATOR.free(kernel_stack_base, KERNEL_STACK_SIZE + PAGE_SIZE);
	if (user_stack_base) {
		static_cast<Thread*>(this)->process->free(reinterpret_cast<usize>(user_stack_base), USER_STACK_SIZE);
	}
	if (simd) {
		auto simd_size = CPU_FEATURES.xsave ? CPU_FEATURES.xsave_area_size : sizeof(FxState);
		KERNEL_VSPACE.free_backed(simd, simd_size);
	}
}
