#pragma once
#include "arch.hpp"
#include "fb.hpp"
#include "interrupts/interrupts.hpp"
#include "noalloc/string.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "utils/spinlock.hpp"

struct PsfFont {
	u32 magic;
	u32 version;
	u32 header_size;
	u32 flags;
	u32 num_glyph;
	u32 bytes_per_glyph;
	u32 height;
	u32 width;
};

enum class Fmt {
	Dec,
	Hex,
	HexNoPrefix,
	Bin
};

struct ZeroPad {
	explicit ZeroPad(usize count = 1) : count {count} {}
	usize count;
};

template<typename T>
inline void print_nolock(T value);

template<>
inline void print_nolock(Fmt value) {
	extern void set_fmt(Fmt fmt);
	set_fmt(value);
}

template<>
inline void print_nolock(ZeroPad value) {
	extern void set_zero_pad(ZeroPad pad);
	set_zero_pad(value);
}

template<typename T>
inline void print_nolock(T* value) {
	extern void print_number(usize value, bool sign);
	extern Fmt get_fmt();
	auto fmt = get_fmt();
	print_nolock(Fmt::Hex);
	print_number(cast<usize>(value), false);
	print_nolock(fmt);
}

template<>
inline void print_nolock(const noalloc::String& value) {
	extern void print_string(const noalloc::String& str);
	print_string(value);
}

template<>
inline void print_nolock(u8 value) {
	extern void print_number(usize value, bool sign);
	print_number(value, false);
}
template<>
inline void print_nolock(u16 value) {
	extern void print_number(usize value, bool sign);
	print_number(value, false);
}
template<>
inline void print_nolock(u32 value) {
	extern void print_number(usize value, bool sign);
	print_number(value, false);
}
template<>
inline void print_nolock(u64 value) {
	extern void print_number(usize value, bool sign);
	print_number(value, false);
}

template<>
inline void print_nolock(bool value) {
	extern void print_string(const noalloc::String& str);
	print_string(value ? "true" : "false");
}
template<>
inline void print_nolock(const char* value) {
	extern void print_string(const noalloc::String& str);
	print_string(value);
}
template<>
inline void print_nolock(noalloc::String value) {
	extern void print_string(const noalloc::String& str);
	print_string(value);
}

template<>
inline void print_nolock(i8 value) {
	extern void print_number(usize value, bool sign);
	bool sign = value < 0;
	print_number(as<usize>(value), sign);
}
template<>
inline void print_nolock(i16 value) {
	extern void print_number(usize value, bool sign);
	bool sign = value < 0;
	print_number(as<usize>(value), sign);
}
template<>
inline void print_nolock(i32 value) {
	extern void print_number(usize value, bool sign);
	bool sign = value < 0;
	print_number(as<usize>(value), sign);
}
template<>
inline void print_nolock(i64 value) {
	extern void print_number(usize value, bool sign);
	bool sign = value < 0;
	print_number(as<usize>(value), sign);
}

extern Spinlock print_lock;

template<typename... Args>
inline void print_nolock(Args... args) {
	(print_nolock(args), ...);
}

template<typename... Args>
inline void print(Args... args) {
	auto flags = enter_critical();
	print_lock.lock();
	(print_nolock(args), ...);
	print_lock.unlock();
	leave_critical(flags);
}

template<typename... Args>
inline void println(Args... args) {
	auto flags = enter_critical();
	print_lock.lock();
	(print_nolock(args), ...);
	print_nolock("\n");
	print_lock.unlock();
	leave_critical(flags);
}

template<typename... Args>
inline void println_nolock(Args... args) {
	(print_nolock(args), ...);
	print_nolock("\n");
}

void init_console(const Framebuffer* framebuffer, const PsfFont* font);
void set_fg(u32 color);
void set_bg(u32 color);

template<typename... Args>
[[noreturn]] inline void panic(Args... args) {
	set_fg(0xFF0000);
	disable_interrupts();
	print_lock.lock();
	print_nolock("KERNEL PANIC: ");
	print_nolock(args...);
	print_nolock("\n");
	print_lock.unlock();
	while (true) {
		arch_hlt();
	}
}

#define unreachable(msg) panic("entered unreachable code in ", __FILE_NAME__, ":", __LINE__, " (", __func__, ")")