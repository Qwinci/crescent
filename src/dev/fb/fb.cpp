#include "fb.hpp"
#include "font.hpp"
#include "cstring.hpp"

constexpr u32 FONT_WIDTH = 8;
constexpr u32 FONT_HEIGHT = 8;
constexpr u32 LINE_HEIGHT = 12;

Framebuffer::Framebuffer(usize phys, u32 width, u32 height, usize pitch, u32 bpp)
	: phys {phys}, space {phys, pitch * height}, pitch {pitch}, width {width}, height {height}, bpp {bpp} {
	assert(space.map(CacheMode::WriteCombine));
	LOG.lock()->register_sink(this);
}

void Framebuffer::write(kstd::string_view str) {
	for (auto c : str) {
		if (c == '\n') {
			column = 0;
			++row;
			continue;
		}
		else if (c == '\t') {
			column += 4 - (column % 4);
			continue;
		}

		if ((column + 1) * FONT_WIDTH * scale >= width) {
			column = 0;
			++row;
		}
		if (row * LINE_HEIGHT * scale >= height) {
			column = 0;
			row = 0;
			clear();
		}

		auto* ptr = font8x8_basic[static_cast<unsigned char>(c)];
		auto start_x = column * FONT_WIDTH * scale;
		auto start_y = row * LINE_HEIGHT * scale;
		for (u32 y = 0; y < FONT_HEIGHT * scale; ++y) {
			auto font_row = ptr[y / scale];
			for (u32 x = 0; x < FONT_WIDTH * scale; ++x) {
				auto* pixel_ptr = reinterpret_cast<uint32_t*>(static_cast<u8*>(space.mapping()) + (start_y + y) * pitch + (start_x + x) * 4);
				if (font_row >> (x / scale) & 1) {
					*pixel_ptr = fg_color;
				}
			}
		}
		++column;
	}
}

void Framebuffer::set_color(Color color) {
	switch (color) {
		case Color::Red:
			fg_color = 0xFF0000;
			break;
		case Color::White:
			fg_color = 0xFFFFFF;
			break;
		case Color::Reset:
			fg_color = 0x00FF00;
			break;
	}
}

void Framebuffer::clear() {
	memset(space.mapping(), 0, pitch * height);
}

ManuallyInit<Framebuffer> BOOT_FB;
