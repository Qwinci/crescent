#pragma once
#include "string_view.hpp"
#include "types.hpp"
#include "double_list.hpp"
#include "utils/spinlock.hpp"
#include "utils/irq_guard.hpp"
#include "arch/misc.hpp"

enum class Color {
	Red,
	Reset
};

struct LogSink {
	DoubleListHook log_hook {};
	bool registed {};

	virtual void write(kstd::string_view str) = 0;
	virtual void set_color(Color) {}
};

enum class Fmt {
	Dec,
	Hex,
	Bin,
	Reset
};

struct Pad {
	constexpr Pad() : c {' '}, amount {0} {}
	constexpr Pad(char c, u32 amount) : c {c}, amount {amount} {}
	char c;
	u32 amount;
};

constexpr Pad zero_pad(u32 amount) {
	return Pad {'0', amount};
}

class Log {
public:
	Log& operator<<(usize value);
	Log& operator<<(kstd::string_view str);
	Log& operator<<(Fmt new_fmt);
	Log& operator<<(Color new_color);
	Log& operator<<(Pad new_pad);

	void register_sink(LogSink* sink);
	void unregister_sink(LogSink* sink);

private:
	static constexpr usize LOG_SIZE = 0x2000;

	DoubleList<LogSink, &LogSink::log_hook> sinks{};
	char buf[LOG_SIZE] {};
	usize buf_ptr {};
	Fmt fmt {};
	Pad pad {};
};

extern Spinlock<Log> LOG;

template<typename... Args>
inline void print(Args... args) {
	IrqGuard irq_guard {};
	auto guard = LOG.lock();
	((*guard << args), ...);
}

template<typename... Args>
inline void println(Args... args) {
	IrqGuard irq_guard {};
	auto guard = LOG.lock();
	((*guard << args), ...);
	*guard << kstd::string_view {"\n"};
}

template<typename... Args>
[[noreturn]] inline void panic(Args... args) {
	IrqGuard irq_guard {};
	println("KERNEL PANIC: ", args...);
	while (true) {
		arch_hlt();
	}
}
