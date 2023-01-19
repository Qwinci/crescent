#pragma once
#include "types.hpp"
#include "utils.hpp"
#include "fb.hpp"

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
	Bin
};

template<typename T>
inline void print(T value);

template<>
inline void print(Fmt value) {
	extern void set_fmt(Fmt fmt);
	set_fmt(value);
}

template<>
inline void print(const char* value) {
	extern void print_string(const char* str);
	print_string(value);
}

template<>
inline void print(void* value) {
	extern void print_number(usize value, bool sign);
	extern Fmt get_fmt();
	auto fmt = get_fmt();
	print(Fmt::Hex);
	print_number(cast<usize>(value), false);
	print(fmt);
}

template<>
inline void print(u8 value) {
	extern void print_number(usize value, bool sign);
	print_number(value, false);
}
template<>
inline void print(u16 value) {
	extern void print_number(usize value, bool sign);
	print_number(value, false);
}
template<>
inline void print(u32 value) {
	extern void print_number(usize value, bool sign);
	print_number(value, false);
}
template<>
inline void print(u64 value) {
	extern void print_number(usize value, bool sign);
	print_number(value, false);
}

template<>
inline void print(bool value) {
	extern void print_string(const char* str);
	print_string(value ? "true" : "false");
}

template<>
inline void print(i8 value) {
	extern void print_number(usize value, bool sign);
	bool sign = value < 0;
	print_number(as<usize>(value), sign);
}
template<>
inline void print(i16 value) {
	extern void print_number(usize value, bool sign);
	bool sign = value < 0;
	print_number(as<usize>(value), sign);
}
template<>
inline void print(i32 value) {
	extern void print_number(usize value, bool sign);
	bool sign = value < 0;
	print_number(as<usize>(value), sign);
}
template<>
inline void print(i64 value) {
	extern void print_number(usize value, bool sign);
	bool sign = value < 0;
	print_number(as<usize>(value), sign);
}

template<typename... Args>
inline void print(Args... args) {
	(print(args), ...);
}

template<typename... Args>
inline void println(Args... args) {
	(print(args), ...);
	print("\n");
}

void init_console(const Framebuffer* framebuffer, const PsfFont* font);
void set_fg(u32 color);
void set_bg(u32 color);

template<typename... Args>
[[noreturn]] inline void panic(Args... args) {
	set_fg(0xFF0000);
	print("KERNEL PANIC: ");
	print(args...);
	while (true) {
		asm("hlt");
	}
}

#define unreachable(msg) panic("entered unreachable code in ", __FILE_NAME__, ":", __LINE__, " (", __func__, ")")