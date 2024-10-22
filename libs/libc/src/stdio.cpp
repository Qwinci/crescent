#include "stdio.h"
#include "sys.hpp"
#include "string.h"
#include <utility>
#include <new>

template<typename T>
struct NoDestroy {
	template<typename... Args>
	constexpr explicit NoDestroy(Args&&... args) {
		new (data) T {std::forward<Args&&>(args)...};
	}

	constexpr T* operator->() {
		return std::launder(reinterpret_cast<T*>(data));
	}

	constexpr const T* operator->() const {
		return std::launder(reinterpret_cast<const T*>(data));
	}

	constexpr T& operator*() {
		return *std::launder(reinterpret_cast<T*>(data));
	}

	constexpr const T& operator*() const {
		return *std::launder(reinterpret_cast<const T*>(data));
	}

private:
	alignas(alignof(T)) char data[sizeof(T)] {};
};

int puts(const char* str) {
	auto len = strlen(str);
	sys_write(STDOUT_HANDLE, str, 0, len);
	sys_write(STDOUT_HANDLE, "\n", 0, 1);
	return static_cast<int>(len);
}

struct FILE {
	virtual ~FILE() = default;
	virtual int write(const void* data, size_t size) = 0;
};

struct StdFile : public FILE {
	constexpr explicit StdFile(CrescentHandle handle) : handle {handle} {}

	int write(const void* data, size_t size) override {
		sys_write(handle, data, 0, size);
		return 0;
	}

	CrescentHandle handle;
};

namespace {
	NoDestroy<StdFile> STDIN_FILE {STDIN_HANDLE};
	NoDestroy<StdFile> STDOUT_FILE {STDOUT_HANDLE};
	NoDestroy<StdFile> STDERR_FILE {STDERR_HANDLE};
}

FILE* stdin = &*STDIN_FILE;
FILE* stdout = &*STDOUT_FILE;
FILE* stderr = &*STDERR_FILE;

int printf(const char* __restrict fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int ret = vfprintf(&*STDOUT_FILE, fmt, ap);
	va_end(ap);
	return ret;
}

int fprintf(FILE* __restrict file, const char* __restrict fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int ret = vfprintf(file, fmt, ap);
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
			case 'u':
			{
				auto value = va_arg(ap, unsigned int);
				char buf[10];
				char* ptr = buf + 10;
				do {
					*--ptr = static_cast<char>('0' + value % 10);
					value /= 10;
				} while (value);
				file->write(ptr, (buf + 10) - ptr);
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
			case 's':
			{
				auto* ptr = va_arg(ap, const char*);
				auto str_len = strlen(ptr);
				file->write(ptr, str_len);
				break;
			}
		}
		++fmt;
	}

	return static_cast<int>(written);
}
