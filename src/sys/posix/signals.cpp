#include "signals.hpp"
#include "arch/arch_irq.hpp"
#include "posix_sys.hpp"
#include "sched/process.hpp"
#include "sched/sched.hpp"
#include "sched/thread.hpp"
#include "sys/user_access.hpp"

PosixSignalDisposition posix_handle_default_signal(int sig) {
	if (sig == SIGCHLD || sig == SIGURG || sig == SIGWINCH) {
		return PosixSignalDisposition::Ignore;
	}
	else if (sig == SIGCONT) {
		return PosixSignalDisposition::Continue;
	}
	else if (sig == SIGSTOP || sig == SIGTSTP || sig == SIGTTIN || sig == SIGTTOU) {
		return PosixSignalDisposition::Stop;
	}
	else {
		return PosixSignalDisposition::Kill;
	}
}

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

void posix_sigprocmask(SyscallFrame* frame) {
	int how = static_cast<int>(*frame->arg0());
	auto user_set = *frame->arg1();
	auto user_old_set = *frame->arg2();

	auto thread = get_current_thread();

	if (!user_set) {
		if (user_old_set) {
			if (!UserAccessor(user_old_set).store(thread->signal_ctx.blocked_signals)) {
				*frame->error() = EFAULT;
				return;
			}
		}

		*frame->error() = 0;
		return;
	}

	u64 set;
	if (!UserAccessor(user_set).load(set)) {
		*frame->error() = EFAULT;
		return;
	}

	set &= ~(1 << SIGKILL | 1 << SIGSTOP);

	if (user_old_set) {
		u64 old = thread->signal_ctx.blocked_signals;
		if (!UserAccessor(user_old_set).store(old)) {
			*frame->error() = EFAULT;
			return;
		}
	}

	if (how == SIG_BLOCK) {
		thread->signal_ctx.blocked_signals |= set;
	}
	else if (how == SIG_UNBLOCK) {
		thread->signal_ctx.blocked_signals &= ~set;
	}
	else if (how == SIG_SETMASK) {
		thread->signal_ctx.blocked_signals = set;
	}
	else {
		*frame->error() = EINVAL;
		return;
	}

	*frame->error() = 0;
}

void posix_sigaction(SyscallFrame* frame) {
	auto num = static_cast<int>(*frame->arg0());
	auto user_act = *frame->arg1();
	auto user_old_act = *frame->arg2();

	if (num < 0 || num >= 64) {
		*frame->error() = EINVAL;
		return;
	}

	auto guard = get_current_thread()->process->signal_ctx.lock();
	auto& sig = guard->signals[num];

	if (!user_act) {
		if (user_old_act) {
			if (!UserAccessor(user_old_act).store(sig)) {
				*frame->error() = EFAULT;
				return;
			}
		}

		*frame->error() = 0;
		return;
	}

	SignalContext::Signal act {};
	if (!UserAccessor(user_act).load(&act, offsetof(SignalContext::Signal, stack_base))) {
		*frame->error() = EFAULT;
		return;
	}

	if (user_old_act) {
		if (!UserAccessor(user_old_act).store(&sig, offsetof(SignalContext::Signal, stack_base))) {
			*frame->error() = EFAULT;
			return;
		}
	}

	if (num != SIGKILL && num != SIGSTOP) {
		memcpy(&sig, &act, offsetof(SignalContext::Signal, stack_base));
	}

	*frame->error() = 0;
}

void arch_sigrestore(SyscallFrame* frame);

void posix_sigrestore(SyscallFrame* frame) {
	arch_sigrestore(frame);
}

void posix_tgkill(SyscallFrame* frame) {
	auto gid = static_cast<pid_t>(*frame->arg0());
	auto tid = static_cast<pid_t>(*frame->arg1());
	int sig = static_cast<int>(*frame->arg2());

	if (sig < 0 || sig > 64) {
		*frame->error() = EINVAL;
		return;
	}

	auto proc_guard = PID_TO_PROC->lock();

	auto proc_opt = proc_guard->get(gid);
	if (!proc_opt) {
		*frame->error() = ESRCH;
		return;
	}

	auto proc = *proc_opt;

	auto guard = proc->tid_to_thread.lock();
	Thread** thread_opt = guard->get(tid);
	if (!thread_opt) {
		*frame->error() = ESRCH;
		return;
	}

	auto thread = *thread_opt;

	if (sig != 0) {
		auto sig_guard = proc->signal_ctx.lock();
		sig_guard->send_signal(thread, sig, false);
	}

	*frame->error() = 0;
}

void posix_kill(SyscallFrame* frame) {
	auto gid = static_cast<pid_t>(*frame->arg0());
	int sig = static_cast<int>(*frame->arg2());

	if (sig < 0 || sig > 64) {
		*frame->error() = EINVAL;
		return;
	}

	auto proc_guard = PID_TO_PROC->lock();

	auto proc_opt = proc_guard->get(gid);
	if (!proc_opt) {
		*frame->error() = ESRCH;
		return;
	}

	if (sig == 0) {
		*frame->error() = 0;
		return;
	}

	auto proc = *proc_opt;

	auto sig_guard = proc->signal_ctx.lock();

	IrqGuard irq_guard {};
	auto guard = proc->threads.lock();
	for (auto& thread : *guard) {
		if (thread.exited) {
			continue;
		}

		if (!sig_guard->send_signal(&thread, sig, true)) {
			continue;
		}

		if (sig != SIGSTOP) {
			break;
		}
	}

	*frame->error() = 0;
}
