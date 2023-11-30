#include "stdio.h"
#include "arch/interrupts.h"
#include "arch/misc.h"
#include "dev/log.h"
#include "inttypes.h"
#include "string.h"
#include "utils/spinlock.h"
#include <stdarg.h>

void kputs_nolock(const char* str, usize len) {
	// todo no lock
	log_print(str_new_with_len(str, len));
}

void kputs(const char* str, usize len) {
	Ipl old = arch_ipl_set(IPL_CRITICAL);
	kputs_nolock(str, len);
	arch_ipl_set(old);
}

static const char hex_digits[] = "0123456789ABCDEF";

static char* fmt_b(usize value, char* str) {
	if (!value) {
		*--str = '0';
	}
	for (; value; value >>= 1) {
		*--str = (char) ('0' + (value & 1));
	}
	return str;
}

static char* fmt_x(usize value, char* str, int lower) {
	if (!value) {
		*--str = '0';
	}
	for (; value; value >>= 4) {
		*--str = (char) (hex_digits[value & 15] | lower);
	}
	return str;
}

static char* fmt_u(usize value, char* str) {
	if (!value) {
		*--str = '0';
	}
	for (; value; value /= 10) {
		*--str = (char) ('0' + value % 10);
	}
	return str;
}

void kvprintf_nolock(const char* fmt, va_list valist) {
	char buffer[101];
	buffer[64] = 0;

	while (true) {
		if (!*fmt) {
			break;
		}

		const char* start = fmt;
		for (; *fmt && *fmt != '%'; ++fmt);
		usize percent_count = 0;
		for (; fmt[0] == '%' && fmt[1] == '%'; ++percent_count, fmt += 2);
		usize len = fmt - start - percent_count;
		kputs_nolock(start, len);
		if (len) {
			continue;
		}
		fmt += 1;

#define FLAG_LJUST (1U << ('-' - ' '))
#define FLAG_SIGN (1U << ('+' - ' '))
#define FLAG_SIGN_SPACE (1U << (' ' - ' '))
#define FLAG_ALT (1U << ('#' - ' '))
#define FLAG_ZERO (1U << ('0' - ' '))
#define FLAG_MASK (FLAG_LJUST | FLAG_SIGN | FLAG_SIGN_SPACE | FLAG_ALT | FLAG_ZERO)

		u32 flags = 0;
		for (; *fmt - ' ' < 32 && 1U << (*fmt - ' ') & FLAG_MASK; ++fmt) {
			flags |= 1U << (*fmt - ' ');
		}

		usize min_width = 0;
		if (*fmt == '*') {
			++fmt;
			int min_width_arg = va_arg(valist, int);
			if (min_width_arg < 0) {
				flags |= FLAG_LJUST;
				min_width_arg *= -1;
			}
			min_width = (usize) min_width_arg;
		}
		else {
			const char* min_width_end;
			min_width = strtoumax(fmt, &min_width_end, 10);
			fmt = min_width_end;
		}

		usize precision = 1;
		bool default_precision = true;
		if (*fmt == '.') {
			default_precision = false;
			++fmt;
			if (*fmt == '*') {
				++fmt;
				int prec_arg = va_arg(valist, int);
				if (prec_arg > 0) {
					precision = (usize) prec_arg;
				}
			}
			else {
				const char* prec_end;
				precision = strtoumax(fmt, &prec_end, 10);
				fmt = prec_end;
			}
		}

		char* ptr = buffer + sizeof(buffer) - 2;
		len = 0;
		if (*fmt == 'p') {
			void* value = va_arg(valist, void*);
			ptr = fmt_x((usize) value, ptr, 0);
			len = (buffer + sizeof(buffer) - 2) - ptr;
		}
		else if (*fmt == 's') {
			const char* value = va_arg(valist, const char*);
			kputs_nolock(value, !default_precision ? precision : strlen(value));
		}
		else if (*fmt == 'c') {
			char c = va_arg(valist, int);
			kputs_nolock(&c, 1);
		}
		else if (*fmt == 'd' || *fmt == 'i') {
			isize value = va_arg(valist, int);
			bool sign = false;
			if (value < 0) {
				value *= -1;
				sign = true;
			}
			ptr = fmt_u((usize) value, ptr);
			if (sign) {
				*--ptr = '-';
			}
			len += (buffer + sizeof(buffer) - 2) - ptr;
		}
		else if (*fmt == 'u') {
			usize value = va_arg(valist, usize);
			ptr = fmt_u((usize) value, ptr);
			len = (buffer + sizeof(buffer) - 2) - ptr;
		}
		else if (*fmt == 'x' || *fmt == 'X') {
			usize value = va_arg(valist, usize);
			if (!(precision == 0 && value == 0)) {
				ptr = fmt_x(value, ptr, *fmt & 1 << 5);
				len = (buffer + sizeof(buffer) - 2) - ptr;
				if (value == 0 && flags & FLAG_ALT) {
					*--ptr = (*fmt & 1 << 5) ? 'x' : 'X';
					*--ptr = '0';
				}
			}
		}
		else if (*fmt == 'b' || *fmt == 'B') {
			usize value = va_arg(valist, usize);
			ptr = fmt_b(value, ptr);
			len = (buffer + sizeof(buffer) - 2) - ptr;
		}
		else if (*fmt == 'f' && fmt[1] == 'g') {
			char buf[5] = {-1};
			u32 fg = va_arg(valist, u32);
			memcpy(buf + 1, &fg, 4);
			kputs_nolock(buf, 5);
			fmt += 1;
		}
		else if (*fmt == 'b' && fmt[1] == 'g') {
			char buf[5] = {-2};
			u32 bg = va_arg(valist, u32);
			memcpy(buf + 1, &bg, 4);
			kputs_nolock(buf, 5);
			fmt += 1;
		}
		fmt += 1;

		if (len < min_width && flags & FLAG_LJUST) {
			kputs_nolock(ptr, len);
			ptr = buffer + sizeof(buffer) - 2;

			char pad_char = flags & FLAG_ZERO ? '0' : ' ';
			usize pad_len = 0;
			for (usize i = len; i < min_width && ptr > buffer; ++i, ++pad_len) {
				*--ptr = pad_char;
			}
			len = pad_len;
		}
		else if (len < min_width) {
			char pad_char = flags & FLAG_ZERO ? '0' : ' ';
			for (usize i = len; i < min_width && ptr > buffer; ++i) {
				*--ptr = pad_char;
			}
			len = min_width;
		}

		kputs_nolock(ptr, len);
	}
}

void kvprintf(const char* fmt, va_list valist) {
	Ipl old = arch_ipl_set(IPL_CRITICAL);
	kvprintf_nolock(fmt, valist);
	arch_ipl_set(old);
}

void kprintf(const char* fmt, ...) {
	va_list valist;
	va_start(valist, fmt);
	kvprintf(fmt, valist);
	va_end(valist);
}

void kprintf_nolock(const char* fmt, ...) {
	va_list valist;
	va_start(valist, fmt);
	kvprintf_nolock(fmt, valist);
	va_end(valist);
}

NORETURN void panic(const char* fmt, ...) {
	arch_ipl_set(IPL_CRITICAL);
	va_list valist;
	va_start(valist, fmt);
	kprintf_nolock("%" PRIfg, 0xFF0000);
	kputs_nolock("KERNEL PANIC: ", sizeof("KERNEL PANIC: ") - 1);
	kvprintf_nolock(fmt, valist);
	va_end(valist);

	while (true) {
		arch_hlt();
	}
}