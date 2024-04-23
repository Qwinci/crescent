#include "stdio.h"
#include "sys.hpp"
#include "string.h"

int puts(const char* str) {
	auto len = strlen(str);
	sys_syslog(str, len);
	return static_cast<int>(len);
}

struct FILE {
	virtual ~FILE() = default;
	virtual int write(const void* data, size_t size) = 0;
};

struct SysFile : public FILE {
	int write(const void* data, size_t size) override {
		sys_syslog(static_cast<const char*>(data), size);
		return 0;
	}
};

int printf(const char* __restrict fmt, ...) {
	SysFile file {};
	va_list ap;
	va_start(ap, fmt);
	int ret = vfprintf(&file, fmt, ap);
	va_end(ap);
	return ret;
}

static constexpr char HEX_CHARS[] = "0123456789ABCDEF";

int vfprintf(FILE* __restrict file, const char* __restrict fmt, va_list ap) {
	size_t written = 0;

	while (true) {
		auto* start = fmt;
		for (; *fmt && *fmt != '%'; ++fmt);
		size_t len = fmt - start;
		for (; fmt[0] == '%' && fmt[1] == '%'; fmt += 2) ++len;
		if (len) {
			file->write(start, len);
			written += len;
		}

		if (!*fmt) {
			break;
		}
		++fmt;
		switch (*fmt) {
			case 'd':
			{
				int value = va_arg(ap, int);
				bool negative;
				if (value < 0) {
					value *= -1;
					negative = true;
				}
				else {
					negative = false;
				}

				char buf[20];
				char* ptr = buf + 20;
				do {
					*--ptr = static_cast<char>('0' + value % 10);
					value /= 10;
				} while (value);
				if (negative) {
					*--ptr = '-';
				}
				file->write(ptr, (buf + 20) - ptr);
				break;
			}
			case 'p':
			{
				void* ptr = va_arg(ap, void*);
				auto value = reinterpret_cast<uintptr_t>(ptr);
				char buf[16];
				char* buf_ptr = buf + 16;
				do {
					*--buf_ptr = HEX_CHARS[value % 16];
					value /= 16;
				} while (value);
				file->write(buf_ptr, (buf + 16) - buf_ptr);
				break;
			}
		}
		++fmt;
	}

	return static_cast<int>(written);
}
