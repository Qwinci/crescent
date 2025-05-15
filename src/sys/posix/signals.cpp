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
