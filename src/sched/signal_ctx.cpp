#include "signal_ctx.hpp"
#include "arch/arch_irq.hpp"
#include "sys/posix/signals.hpp"
#include "thread.hpp"
#include "process.hpp"

bool arch_handle_signal(IrqFrame* frame, Thread* thread, SignalContext& sig_ctx, int sig);

bool SignalContext::send_signal(Thread* thread, int sig, bool check_mask) {
	auto& s = signals[sig];
	if (s.handler == Signal::IGNORE) {
		return true;
	}
	else if (s.handler == Signal::DEFAULT) {
		auto act = posix_handle_default_signal(sig);
		switch (act) {
			case PosixSignalDisposition::Kill:
				println("[kernel][signal]: killing thread ", thread->name, " because of unhandled signal ", sig);
				thread->exited = true;
				thread->exit(-1);
				break;
			case PosixSignalDisposition::Ignore:
				break;
			case PosixSignalDisposition::Stop:
				break;
			case PosixSignalDisposition::Continue:
				break;
		}

		return true;
	}

	if (check_mask) {
		if (!(thread->signal_ctx.blocked_signals & 1UL << sig)) {
			return false;
		}
	}

	thread->signal_ctx.pending_signals |= 1UL << sig;
	return true;
}

void ThreadSignalContext::check_signals(IrqFrame* frame, Thread* thread) {
	auto guard = thread->process->signal_ctx.lock();

	auto available = pending_signals & (~blocked_signals | SIGKILL | SIGSTOP);
	if (available) {
		for (int i = 0; i < 64; ++i) {
			if (available & 1 << i) {
				if (!arch_handle_signal(frame, thread, *guard, i)) {
					pending_signals &= ~(1 << i);
					if (i == SIGSEGV) {
						println("[kernel][signal]: killing thread ", thread->name, " because of unhandled SIGSEGV while storing signal frame");
						thread->exited = true;
						thread->exit(-1);
					}
					continue;
				}

				auto& s = guard->signals[i];
				blocked_signals |= s.mask;
				if (!(s.flags & SignalFlags::NoDefer)) {
					blocked_signals |= 1 << i;
				}

				pending_signals &= ~(1 << i);
				break;
			}
		}
	}
}
