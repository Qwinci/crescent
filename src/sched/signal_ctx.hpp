#pragma once
#include "types.hpp"
#include "utils/flags_enum.hpp"
#include "arch/irq.hpp"

enum class SignalFlags : unsigned long {
	NoChildStop = 1,
	NoChildWait = 2,
	SigInfo = 4,
	Restorer = 0x4000000,
	AltStack = 0x8000000,
	RestartSyscall = 0x10000000,
	NoDefer = 0x40000000,
	ResetOnEntry = 0x80000000
};
FLAGS_ENUM(SignalFlags);

struct Thread;

struct ThreadSignalContext {
	u64 blocked_signals {};
	u64 pending_signals {};

	void check_signals(IrqFrame* frame, Thread* thread);
};

struct SignalContext {
	bool send_signal(Thread* thread, int sig, bool check_mask);

	struct Signal {
		static constexpr usize DEFAULT = 0;
		static constexpr usize IGNORE = 1;

		usize handler;
		u64 mask;
		SignalFlags flags;
		usize restorer;
		usize stack_base;
		usize stack_size;
	};

	Signal signals[64] {};
};
