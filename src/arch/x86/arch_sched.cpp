#include "arch/arch_irq.hpp"
#include "arch/arch_syscalls.hpp"
#include "arch/arch_thread.hpp"
#include "arch/cpu.hpp"
#include "assert.hpp"
#include "cpu.hpp"
#include "mem/mem.hpp"
#include "mem/vspace.hpp"
#include "sched/sched.hpp"
#include "simd_state.hpp"
#include "sys/user_access.hpp"
#include "sys/posix/posix_sys.hpp"

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

	// prev->move_lock = false
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

static constexpr usize GUARD_SIZE = PAGE_SIZE;

ArchThread::ArchThread(void (*fn)(void* arg), void* arg, Process* process) : self {this} {
	kernel_stack_base = new usize[(KERNEL_STACK_SIZE + GUARD_SIZE) / 8] {};
	syscall_sp = reinterpret_cast<u8*>(kernel_stack_base) + KERNEL_STACK_SIZE + GUARD_SIZE;
	sp = reinterpret_cast<u8*>(kernel_stack_base) + KERNEL_STACK_SIZE + GUARD_SIZE - (process->user ? sizeof(UserInitFrame) : (sizeof(InitFrame) + 8));
	auto* frame = reinterpret_cast<InitFrame*>(sp);
	memset(frame, 0, sizeof(InitFrame));
	frame->rdi = reinterpret_cast<u64>(arg);

	for (usize i = 0; i < GUARD_SIZE; i += PAGE_SIZE) {
		KERNEL_PROCESS->page_map.protect(reinterpret_cast<u64>(kernel_stack_base) + i, PageFlags::Read, CacheMode::WriteBack);
	}

	frame->rip = reinterpret_cast<u64>(fn);
	frame->rflags = 2;

	if (process->user) {
		user_stack_base = reinterpret_cast<u8*>(process->allocate(
			nullptr,
			USER_STACK_SIZE + PAGE_SIZE,
			PageFlags::Read | PageFlags::Write,
			MemoryAllocFlags::Backed,
			nullptr));
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

	kernel_stack_base = new usize[(KERNEL_STACK_SIZE + GUARD_SIZE) / 8] {};
	syscall_sp = reinterpret_cast<u8*>(kernel_stack_base) + KERNEL_STACK_SIZE + GUARD_SIZE;
	sp = reinterpret_cast<u8*>(kernel_stack_base) + KERNEL_STACK_SIZE + GUARD_SIZE - (process->user ? sizeof(UserInitFrame) : (sizeof(InitFrame) + 8));
	auto* frame = reinterpret_cast<InitFrame*>(sp);
	memset(frame, 0, sizeof(InitFrame));

	for (usize i = 0; i < GUARD_SIZE; i += PAGE_SIZE) {
		KERNEL_PROCESS->page_map.protect(reinterpret_cast<u64>(kernel_stack_base) + i, PageFlags::Read, CacheMode::WriteBack);
	}

	frame->rflags = 2;
	frame->rip = sysv.ld_entry;

	user_stack_base = reinterpret_cast<u8*>(process->allocate(
		nullptr,
		USER_STACK_SIZE + PAGE_SIZE,
		PageFlags::Read | PageFlags::Write,
		MemoryAllocFlags::Backed, nullptr));
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
	ALLOCATOR.free(kernel_stack_base, KERNEL_STACK_SIZE + GUARD_SIZE);
	if (user_stack_base) {
		static_cast<Thread*>(this)->process->free(reinterpret_cast<usize>(user_stack_base), USER_STACK_SIZE + PAGE_SIZE);
	}
	if (simd) {
		auto simd_size = CPU_FEATURES.xsave ? CPU_FEATURES.xsave_area_size : sizeof(FxState);
		KERNEL_VSPACE.free_backed(simd, simd_size);
	}
}

struct stack_t {
	void* ss_sp;
	int ss_flags;
	size_t ss_size;
};

#define REG_R8 0
#define REG_R9 1
#define REG_R10 2
#define REG_R11 3
#define REG_R12 4
#define REG_R13 5
#define REG_R14 6
#define REG_R15 7
#define REG_RDI 8
#define REG_RSI 9
#define REG_RBP 10
#define REG_RBX 11
#define REG_RDX 12
#define REG_RAX 13
#define REG_RCX 14
#define REG_RSP 15
#define REG_RIP 16
#define REG_EFL 17
#define REG_CSGSFS 18
#define REG_ERR 19
#define REG_TRAPNO 20
#define REG_OLDMASK 21
#define REG_CR2 22

struct mcontext_t {
	u64 gregs[23];
	usize fpregs;
	u64 unused[8];
};

struct ucontext_t {
	unsigned long uc_flags;
	ucontext_t* uc_link;
	stack_t uc_stack;
	mcontext_t uc_mcontext;
	u64 uc_sigmask;
	usize old_sigmask;
};

bool arch_handle_signal(IrqFrame* frame, Thread* thread, SignalContext& sig_ctx, int sig) {
	ucontext_t uctx {};
	auto& ctx = uctx.uc_mcontext;
	ctx.gregs[REG_RAX] = frame->rax;
	ctx.gregs[REG_RBX] = frame->rbx;
	ctx.gregs[REG_RCX] = frame->rcx;
	ctx.gregs[REG_RDX] = frame->rdx;
	ctx.gregs[REG_RDI] = frame->rdi;
	ctx.gregs[REG_RSI] = frame->rsi;
	ctx.gregs[REG_RBP] = frame->rbp;
	ctx.gregs[REG_RSP] = frame->rsp;
	ctx.gregs[REG_R8] = frame->r8;
	ctx.gregs[REG_R9] = frame->r9;
	ctx.gregs[REG_R10] = frame->r10;
	ctx.gregs[REG_R11] = frame->r11;
	ctx.gregs[REG_R12] = frame->r12;
	ctx.gregs[REG_R13] = frame->r13;
	ctx.gregs[REG_R14] = frame->r14;
	ctx.gregs[REG_R15] = frame->r15;
	ctx.gregs[REG_RIP] = frame->rip;
	ctx.gregs[REG_EFL] = frame->rflags;
	ctx.gregs[REG_CSGSFS] = frame->cs;
	ctx.gregs[REG_ERR] = frame->error;

	auto simd_size = CPU_FEATURES.xsave ? CPU_FEATURES.xsave_area_size : sizeof(FxState);

	auto& s = sig_ctx.signals[sig];

	auto user_sp = s.flags & SignalFlags::AltStack ? (s.stack_base + s.stack_size) : frame->rsp;
	user_sp -= simd_size;
	user_sp = ALIGNDOWN(user_sp, 64);

	if (!UserAccessor(user_sp).store(thread->simd, simd_size)) {
		return false;
	}

	ctx.fpregs = user_sp;

	uctx.old_sigmask = thread->signal_ctx.blocked_signals;

	user_sp -= ALIGNUP(sizeof(ucontext_t), 16);
	if (!UserAccessor(user_sp).store(uctx)) {
		return false;
	}
	auto arg_ptr = user_sp;

	user_sp -= 8;
	if (!UserAccessor(user_sp).store(s.restorer)) {
		return false;
	}

	memset(&frame->r15, 0, 15 * 8);
	frame->rdi = sig;
	frame->rdx = arg_ptr;
	frame->rip = s.handler;
	frame->rsp = user_sp;

	return true;
}

void arch_sigrestore(SyscallFrame* frame) {
	ucontext_t uctx {};

	auto thread = get_current_thread();

	auto user_sp = reinterpret_cast<usize>(thread->saved_user_sp);
	if (!UserAccessor(user_sp).load(uctx)) {
		*frame->error() = EFAULT;
		return;
	}

	auto simd_size = CPU_FEATURES.xsave ? CPU_FEATURES.xsave_area_size : sizeof(FxState);

	auto& ctx = uctx.uc_mcontext;

	thread->signal_ctx.blocked_signals = uctx.old_sigmask;
	if (!UserAccessor(ctx.fpregs).load(thread->simd, simd_size)) {
		*frame->error() = EFAULT;
		return;
	}

	thread->saved_user_sp = reinterpret_cast<u8*>(ctx.gregs[REG_RSP]);

	frame->rax = ctx.gregs[REG_RAX];
	frame->rbx = ctx.gregs[REG_RBX];
	frame->rcx = ctx.gregs[REG_RCX];
	frame->rdx = ctx.gregs[REG_RDX];
	frame->rdi = ctx.gregs[REG_RDI];
	frame->rsi = ctx.gregs[REG_RSI];
	frame->rbp = ctx.gregs[REG_RBP];
	frame->r8 = ctx.gregs[REG_R8];
	frame->r9 = ctx.gregs[REG_R9];
	frame->r10 = ctx.gregs[REG_R10];
	frame->r11 = ctx.gregs[REG_R11];
	frame->r12 = ctx.gregs[REG_R12];
	frame->r13 = ctx.gregs[REG_R13];
	frame->r14 = ctx.gregs[REG_R14];
	frame->r15 = ctx.gregs[REG_R15];
	frame->r11 &= ~0b1000011000110111111111;
	frame->r11 |= ctx.gregs[REG_EFL] & 0b1000011000110111111111;
	frame->rcx = ctx.gregs[REG_RIP];
}
