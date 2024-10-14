#pragma once
#include "string_view.hpp"
#include <stdint.h>

namespace kstd {
	template<typename T>
	concept FormatSink = requires(T value) {
		value += "hello world!";
	};

	struct hex_fmt {
		uint64_t value;
	};

	template<FormatSink Sink>
	struct formatter {
		constexpr explicit formatter(Sink& sink) : sink {sink} {}

		formatter& operator<<(string_view str) {
			sink += str;
			return *this;
		}

		formatter& operator<<(uint64_t value) {
			format_number(value, 10);
			return *this;
		}

		formatter& operator<<(hex_fmt value) {
			format_number(value.value, 16);
			return *this;
		}

	private:
		static constexpr char CHARS[] = "0123456789ABCDEF";

		void format_number(uint64_t value, int base) {
			char buf[64];
			char* ptr = buf + 64;
			do {
				*--ptr = CHARS[value % base];
				value /= base;
			} while (value);
			sink += string_view {ptr, static_cast<size_t>(buf + 64 - ptr)};
		}

		Sink& sink;
	};
}
