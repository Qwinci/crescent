#include "stdio.h"
#include "dev/con.h"
#include "inttypes.h"
#include "string.h"
#include <stdarg.h>

void kputs(const char* str, usize len) {
	if (!kernel_con) {
		return;
	}

	for (; len; --len, ++str) {
		if (*str == '\n') {
			kernel_con->column = 0;
			kernel_con->line += 1;
		}
		else if (*str == '\t') {
			kernel_con->column += 4 - (kernel_con->column % 4);
		}
		else {
			kernel_con->write(kernel_con, *str);
		}
	}
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

static char* fmt_x(usize value, char* str) {
	if (!value) {
		*--str = '0';
	}
	for (; value; value >>= 4) {
		*--str = hex_digits[value & 15];
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

void kprintf(const char* fmt, ...) {
	va_list valist;
	va_start(valist, fmt);
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
		kputs(start, len);
		if (len) {
			continue;
		}
		fmt += 1;

		char pad_char = ' ';
		if (*fmt == '0') {
			pad_char = '0';
			++fmt;
		}

		const char* prec_end;
		usize precision = strtoumax(fmt, &prec_end, 10);
		fmt = prec_end;

		char* ptr = buffer + sizeof(buffer) - 2;
		len = 0;
		if (*fmt == 'p') {
			void* value = va_arg(valist, void*);
			ptr = fmt_x((usize) value, ptr);
			len = (buffer + sizeof(buffer) - 2) - ptr;
		}
		else if (*fmt == 's') {
			const char* value = va_arg(valist, const char*);
			kputs(value, strlen(value));
		}
		else if (*fmt == 'c') {
			char c = va_arg(valist, int);
			kputs(&c, 1);
		}
		else if (*fmt == 'd' || *fmt == 'i') {
			isize value = va_arg(valist, isize);
			if (value < 0) {
				value *= -1;
				*--ptr = '-';
				++len;
			}
			ptr = fmt_u((usize) value, ptr);
			len += (buffer + sizeof(buffer) - 2) - ptr;
		}
		else if (*fmt == 'u') {
			usize value = va_arg(valist, usize);
			ptr = fmt_u((usize) value, ptr);
			len = (buffer + sizeof(buffer) - 2) - ptr;
		}
		else if (*fmt == 'x') {
			usize value = va_arg(valist, usize);
			ptr = fmt_x(value, ptr);
			len = (buffer + sizeof(buffer) - 2) - ptr;
		}
		else if (*fmt == 'b') {
			usize value = va_arg(valist, usize);
			ptr = fmt_b(value, ptr);
			len = (buffer + sizeof(buffer) - 2) - ptr;
		}
		fmt += 1;

		if (len < precision) {
			for (usize i = len; i < precision && ptr >= buffer; ++i) {
				*--ptr = pad_char;
			}
			len = precision;
		}

		kputs(ptr, len);
	}

	va_end(valist);
}