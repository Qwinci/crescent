#include <limits.h>
#include "string.h"
#include "utils/attribs.h"
#include "types.h"
#include "inttypes.h"
#include "ctype.h"

size_t strlen(const char* str) {
	size_t len = 0;
	while (*str++) ++len;
	return len;
}

int strcmp(const char* lhs, const char* rhs) {
	for (;; ++lhs, ++rhs) {
		int res = *lhs - *rhs;
		if (res != 0) {
			return res;
		}
		else if (!*rhs || !*lhs) {
			return 0;
		}
	}
}

int strncmp(const char* lhs, const char* rhs, size_t count) {
	for (; count; ++lhs, ++rhs, --count) {
		int res = *lhs - *rhs;
		if (res != 0 || !*rhs || !*lhs) {
			return res;
		}
	}
	return 0;
}

char* strcpy(char* restrict dest, const char* restrict src) {
	char* dest_ptr = dest;
	while (*src) {
		*dest++ = *src++;
	}
	*dest = 0;
	return dest_ptr;
}

char* strncpy(char* restrict dest, const char* restrict src, size_t count) {
	char* dest_ptr = dest;
	for (; count > 1 && *src; --count) {
		*dest_ptr++ = *src++;
	}
	*dest_ptr = 0;
	return dest;
}

char* strcat(char* restrict dest, const char* restrict src) {
	char* dest_ptr = dest;
	size_t len = strlen(dest);
	dest += len;

	while (*src) {
		*dest++ = *src++;
	}
	*dest = 0;
	return dest_ptr;
}

int memcmp(const void* lhs, const void* rhs, size_t count) {
	const unsigned char* lhs_ptr = (const unsigned char*) lhs;
	const unsigned char* rhs_ptr = (const unsigned char*) rhs;
	for (; count; ++lhs_ptr, ++rhs_ptr, --count) {
		int res = *lhs_ptr - *rhs_ptr;
		if (res != 0) {
			return res;
		}
	}
	return 0;
}

WEAK void* memset(void* dest, int ch, size_t count) {
	unsigned char* dest_ptr = (unsigned char*) dest;
	for (; count; --count) {
		*dest_ptr++ = (unsigned char) ch;
	}
	return dest;
}

WEAK void* memcpy(void* dest, const void* restrict src, size_t count) {
	unsigned char* dest_ptr = (unsigned char*) dest;
	const unsigned char* src_ptr = (const unsigned char*) src;
	for (; count; --count) {
		*dest_ptr++ = *src_ptr++;
	}
	return dest;
}

void* memmove(void* dest, const void* src, size_t count) {
	if (dest < src) {
		return memcpy(dest, src, count);
	}
	unsigned char* dest_ptr = (unsigned char*) dest + count;
	const unsigned char* src_ptr = (unsigned char*) src + count;
	for (; count; --count) {
		*--dest_ptr = *--src_ptr;
	}
	return dest;
}

static int print_int(uintmax_t value, char* buffer, size_t max_size, int pad) {
	char buf[20];
	char* ptr = buf + 20;
	do {
		*--ptr = (char) ('0' + value % 10);
		value /= 10;
	} while (value);

	int written = (int) (buf + 20 - ptr);
	if ((size_t) written < max_size) {
		if (written < pad) {
			memset(buffer, '0', pad - written);
			buffer += pad - written;
			memcpy(buffer, ptr, written);
			return pad;
		}
		else {
			memcpy(buffer, ptr, written);
		}
	}
	return written;
}

static int print_hex(uintmax_t value, char* buffer, size_t max_size, int pad) {
	char buf[16];
	char* ptr = buf + 16;
	static char hex_chars[] = "0123456789ABCDEF";
	do {
		*--ptr = hex_chars[value & 15];
		value >>= 4;
	} while (value);

	int written = (int) (buf + 16 - ptr);
	if ((size_t) written < max_size) {
		if (written < pad) {
			memset(buffer, '0', pad - written);
			buffer += pad - written;
			memcpy(buffer, ptr, written);
			return pad;
		}
		else {
			memcpy(buffer, ptr, written);
		}
	}
	return written;
}

static int print_binary(uintmax_t value, char* buffer, size_t max_size, int pad) {
	char buf[sizeof(uintmax_t) * 8];
	char* ptr = buf + sizeof(uintmax_t) * 8;
	do {
		*--ptr = (char) ('0' + (value & 1));
		value >>= 4;
	} while (value);

	int written = (int) (buf + sizeof(uintmax_t) * 8 - ptr);
	if ((size_t) written < max_size) {
		if (written < pad) {
			memset(buffer, '0', pad - written);
			buffer += pad - written;
			memcpy(buffer, ptr, written);
			return pad;
		}
		else {
			memcpy(buffer, ptr, written);
		}
	}
	return written;
}

int vsnprintf(char* restrict buffer, size_t size, const char* restrict fmt, va_list ap) {
	char* ptr = buffer;

	int written = 0;
	while (true) {
		const char* start = fmt;
		for (; *fmt && *fmt != '%'; ++fmt);
		usize len = fmt - start;
		if (buffer && len < size) {
			memcpy(ptr, start, len);
			ptr += len;
			size -= len;
		}
		written += (int) len;
		start = fmt;
		for (; fmt[0] == '%' && fmt[1] == '%'; fmt += 2);
		len = fmt - start;
		if (buffer && len / 2 < size) {
			memcpy(ptr, start, len / 2);
			ptr += len / 2;
			size -= len / 2;
		}

		if (!*fmt) {
			return written;
		}
		++fmt;

		int width;
		if (isdigit(*fmt)) {
			width = (int) strtoumax(fmt, &start, 10);
			fmt = start;
		} else if (*fmt == '*') {
			width = va_arg(ap, int);
			++fmt;
		}
		int precision = 0;
		if (*fmt == '.') {
			++fmt;
			if (isdigit(*fmt)) {
				precision = (int) strtoumax(fmt, &start, 10);
				fmt = start;
			}
			else if (*fmt == '*') {
				precision = va_arg(ap, int);
			}
		}

		switch (*fmt) {
			case 'c':
			{
				char value = (char) va_arg(ap, int);
				if (buffer && size > 1) {
					*ptr++ = value;
					size -= 1;
				}
				written += 1;
				break;
			}
			case 's':
			{
				if (!precision) {
					precision = INT_MAX;
				}
				const char* value = va_arg(ap, const char*);
				for (; precision && *value; --precision, ++value) {
					if (buffer && size) {
						*ptr++ = *value;
						size -= 1;
					}
					written += 1;
				}
				break;
			}
			case 'd':
			case 'i':
			{
				int value = va_arg(ap, int);
				if (value < 0) {
					if (buffer && size > 1) {
						*ptr++ = '-';
						size -= 1;
					}
					written += 1;
					value *= -1;
				}
				written += print_int((uintmax_t) value, ptr, size, width);
				break;
			}
			case 'u':
			{
				unsigned int value = va_arg(ap, unsigned int);
				written += print_int(value, ptr, size, width);
				break;
			}
			case 'x':
			{
				uintmax_t value = va_arg(ap, uintmax_t);
				written += print_hex(value, ptr, size, width);
				break;
			}
			case 'b':
			{
				uintmax_t value = va_arg(ap, uintmax_t);
				written += print_binary(value, ptr, size, width);
				break;
			}
			case 'p':
			{
				void* value = va_arg(ap, void*);
				written += print_binary((uintmax_t) value, ptr, size, width);
				break;
			}
			default:
				if (buffer && size > 1) {
					*ptr++ = *fmt;
					size -= 1;
				}
				written += 1;
				break;
		}
		++fmt;
	}
}

int snprintf(char* restrict buffer, size_t size, const char* restrict fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int ret = vsnprintf(buffer, size, fmt, ap);
	va_end(ap);
	return ret;
}
