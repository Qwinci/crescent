#pragma once
#include "types.hpp"
#include "utils.hpp"

struct Framebuffer {
	usize address;
	usize width;
	usize height;
	usize pitch;
	usize bpp;

	inline void write(usize x, usize y, u32 color) const {
		*cast<u32*>(address + y * pitch + x * bpp) = color;
	}

	[[nodiscard]] inline u32 read(usize x, usize y) const {
		return *cast<u32*>(address + y * pitch + x * bpp);
	}

	inline void clear(u32 color) const {
		for (usize y = 0; y < height; ++y) {
			for (usize x = 0; x < width; ++x) {
				write(x, y, color);
			}
		}
	}
};