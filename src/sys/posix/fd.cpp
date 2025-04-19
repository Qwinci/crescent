#include "dev/clock.hpp"
#include "posix_sys.hpp"
#include "sched/sched.hpp"
#include "sched/process.hpp"
#include "sys/user_access.hpp"
#include "vector.hpp"

#define POLLIN 1
#define POLLPRI 2
#define POLLOUT 4
#define POLLERR 8
#define POLLHUP 0x10
#define POLLNVAL 0x20
#define POLLRDNORM 0x40
#define POLLRDBAND 0x80
#define POLLWRNORM 0x100
#define POLLWRBAND 0x200
#define POLLMSG 0x400
#define POLLREMOVE 0x1000
#define POLLRDHUP 0x2000

using nfds_t = unsigned long;

struct pollfd {
	int fd;
	short events;
	short revents;
};

void posix_ppoll(SyscallFrame* frame) {
	auto user_fds = *frame->arg0();
	auto num_fds = static_cast<nfds_t>(*frame->arg1());
	auto user_timeout = *frame->arg2();
	auto user_sig_mask = *frame->arg3();

	kstd::vector<pollfd> fds;
	fds.resize(num_fds);
	if (!UserAccessor(user_fds).load(fds.data(), num_fds * sizeof(pollfd))) {
		*frame->error() = EFAULT;
		return;
	}

	u64 timeout_ns = UINT64_MAX;

	if (user_timeout) {
		timespec64 timeout {};
		if (!UserAccessor(user_timeout).load(timeout)) {
			*frame->error() = EFAULT;
			return;
		}
		timeout_ns = timeout.tv_sec * NS_IN_S + timeout.tv_nsec;
	}

	auto thread = get_current_thread();
	auto proc = thread->process;

	u64 orig_sig_mask {};

	if (user_sig_mask) {
		sigset_t sig_mask {};
		if (!UserAccessor(user_sig_mask).load(sig_mask)) {
			*frame->error() = EFAULT;
			return;
		}

		auto sig_guard = proc->signal_ctx.lock();
		orig_sig_mask = thread->signal_ctx.blocked_signals;
		thread->signal_ctx.blocked_signals = sig_mask.value;
	}

	kstd::vector<Event*> poll_events;
	kstd::vector<kstd::shared_ptr<OpenFile>> poll_files;
	poll_events.resize(fds.size());
	poll_files.resize(fds.size());

	usize actual_poll_events = 0;
	int ret = 0;
	for (int i = 0; i < 2; ++i) {
		for (auto& fd : fds) {
			bool found = false;

			fd.revents = 0;

			if (fd.fd < 0 || fd.events == 0) {
				continue;
			}

			auto handle = proc->handles.get(static_cast<CrescentHandle>(fd.fd));
			kstd::shared_ptr<OpenFile>* file_ptr;
			if (!handle || !(file_ptr = handle->get<kstd::shared_ptr<OpenFile>>())) {
				fd.revents |= POLLNVAL;
				++ret;
				continue;
			}

			auto& file = *file_ptr;
			PollEvent events {};
			auto status = file->node->poll(events);
			if (status != FsStatus::Success) {
				fd.revents |= POLLNVAL;
				found = true;
			}

			if (events == PollEvent::None && i == 0) {
				poll_events[actual_poll_events++] = &file->node->poll_event;
				poll_files[actual_poll_events - 1] = file;
			}
			else {
				if (events & PollEvent::In && fd.events & POLLIN) {
					fd.revents |= POLLIN;
					found = true;
				}
				if (events & PollEvent::UrgentOut && fd.events & POLLPRI) {
					fd.revents |= POLLPRI;
					found = true;
				}
				if (events & PollEvent::Out && fd.events & POLLOUT) {
					fd.revents |= POLLOUT;
					found = true;
				}
				if (events & PollEvent::Err) {
					fd.revents |= POLLERR;
					found = true;
				}
				if (events & PollEvent::Hup) {
					fd.revents |= POLLHUP;
					found = true;
				}
			}

			if (found) {
				++ret;
			}
		}

		if (ret || i == 1) {
			break;
		}

		if (!Event::wait_any(poll_events.data(), actual_poll_events, timeout_ns)) {
			break;
		}
	}

	if (user_sig_mask) {
		auto sig_guard = proc->signal_ctx.lock();
		thread->signal_ctx.blocked_signals = orig_sig_mask;
	}

	if (!UserAccessor(user_fds).store(fds.data(), num_fds * sizeof(pollfd))) {
		*frame->error() = EFAULT;
		return;
	}

	*frame->error() = 0;
	*frame->ret() = ret;
}

#define _IOC_NONE 0U
#define _IOC_WRITE 1U
#define _IOC_READ 2U

#define _IOC(dir, type, num, size) (((dir) << 30) | ((size) << 16) | ((type) << 8) | (num))

#define _IO(type, num) _IOC(_IOC_NONE, (type), (num), 0)
#define _IOR(type, num, size) _IOC(_IOC_READ, (type), (num), sizeof(size))
#define _IOW(type, num, size) _IOC(_IOC_WRITE, (type), (num), sizeof(size))
#define _IOWR(type, num, size) _IOC(_IOC_READ | _IOC_WRITE, (type), (num), sizeof(size))

struct winsize {
	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel;
	unsigned short ws_ypixel;
};

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

#define NCCS 32

struct termios {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t c_line;
	cc_t c_cc[NCCS];
};

void posix_ioctl(SyscallFrame* frame) {
	int fd = static_cast<int>(*frame->arg0());
	auto op = static_cast<usize>(*frame->arg1());
	auto user_arg = *frame->arg2();

	println("ioctl ", Fmt::Hex, op, Fmt::Reset);
	if (op == 0x5413) {
		winsize size {
			.ws_row = 18,
			.ws_col = 80,
			.ws_xpixel = 1920,
			.ws_ypixel = 1080
		};
		if (!UserAccessor(user_arg).store(size)) {
			*frame->error() = EFAULT;
			return;
		}
	}
	else if (op == 0x5401) {
		termios term {
			.c_iflag = 0x6B02,
			.c_oflag = 0x3,
			.c_cflag = 0x4B00,
			.c_lflag = 0x200005cb,
			.c_line = 0,
			.c_cc {}
		};
		if (!UserAccessor(user_arg).store(term)) {
			*frame->error() = EFAULT;
			return;
		}
	}

	*frame->error() = 0;
	*frame->ret() = 0;
}

void posix_fcntl(SyscallFrame* frame) {
	int fd = static_cast<int>(*frame->arg0());
	int cmd = static_cast<int>(*frame->arg1());
	auto user_arg = *frame->arg2();

	println("fcntl ", Fmt::Hex, cmd, Fmt::Reset);

	*frame->error() = 0;
	*frame->ret() = 0;
}
