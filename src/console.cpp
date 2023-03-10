#include "console.hpp"
#include "memory/vmem.hpp"

Spinlock print_lock {};

namespace {
	u32 fg;
	u32 bg;
	const Framebuffer* fb;
	const PsfFont* f;
	usize column;
	usize line;
	Fmt fmt;
}

static inline void print_char(char c) {
	if (column * (f->width + 1) + f->width >= fb->width) {
		column = 0;
		line += 1;
	}
	if (line * f->height >= fb->height) {
		fb->clear(bg);
		line = 0;
	}

	u32 bytes_per_line = (f->width + 7) / 8;

	const u8* font_c = cast<const u8*>(f) + f->header_size + f->bytes_per_glyph * c;
	usize init_x = column * (f->width + 1);
	usize init_y = line * f->height;
	for (usize y = 0; y < f->height; ++y) {
		u32 font_line = *cast<const u32*>(font_c);
		for (usize x = 0; x < f->width; ++x) {
			u32 color = (font_line & (1ULL << (f->width - 1 - x))) ? fg : bg;
			fb->write(init_x + x, init_y + y, color);
		}
		font_c += bytes_per_line;
	}

	column += 1;
}

[[gnu::used]] void print_string(const noalloc::String& str) {
	for (usize i = 0; i < str.len(); ++i) {
		if (str[i] == '\t') {
			column += 4 - column % 4;
		}
		else if (str[i] == '\n') {
			column = 0;
			line += 1;
		}
		else {
			print_char(str[i]);
		}
	}
}

static const char hex_chars[] = "0123456789ABCDEF";

static ZeroPad z_pad;

[[gnu::used]] void print_number(usize value, bool sign) {
	if (sign) print_char('-');
	if (fmt == Fmt::Dec) {
		char str[21] {};
		char* ptr = str + 19;
		if (value == 0) {
			*ptr-- = '0';
		}
		while (value) {
			char c = as<char>('0' + value % 10);
			*ptr-- = c;
			value /= 10;
		}
		if (auto pad = (20 - (ptr + 1 - str)); pad < z_pad.count) {
			while (pad < z_pad.count && ptr >= str) {
				*ptr-- = '0';
				++pad;
			}
		}
		print_string(ptr + 1);
	}
	else if (fmt == Fmt::Hex || fmt == Fmt::HexNoPrefix) {
		char str[19] {};
		char* ptr = str + 17;
		if (value == 0) {
			*ptr-- = '0';
		}
		while (value) {
			char c = hex_chars[value % 16];
			*ptr-- = c;
			value /= 16;
		}
		if (auto pad = (18 - (ptr + 1 - str)); pad < z_pad.count) {
			while (pad < z_pad.count && ptr - 2 >= str) {
				*ptr-- = '0';
				++pad;
			}
		}
		if (fmt == Fmt::Hex) {
			*ptr-- = 'x';
			*ptr = '0';
		}
		else {
			ptr++;
		}
		print_string(ptr);
	}
	else if (fmt == Fmt::Bin) {
		char str[67] {};
		char* ptr = str + 65;
		if (value == 0) {
			*ptr-- = '0';
		}
		while (value) {
			char c = as<char>('0' + value % 2);
			*ptr-- = c;
			value /= 2;
		}
		if (auto pad = (66 - (ptr + 1 - str)); pad < z_pad.count) {
			while (pad < z_pad.count && ptr - 2 >= str) {
				*ptr-- = '0';
				++pad;
			}
		}
		*ptr-- = 'b';
		*ptr = '0';
		print_string(ptr);
	}
}

void init_console(const Framebuffer* framebuffer, const PsfFont* font) {
	fb = framebuffer;
	f = font;
}

void set_fg(u32 color) {
	fg = color;
}

void set_bg(u32 color) {
	bg = color;
}

void set_fmt(Fmt new_fmt) {
	fmt = new_fmt;
}

void set_zero_pad(ZeroPad pad) {
	z_pad = pad;
}

Fmt get_fmt() {
	return fmt;
}