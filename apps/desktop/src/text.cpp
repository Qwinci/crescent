#include "text.hpp"

TextWindow::TextWindow()
	: Window {true}, text_ctx {libtext::Context::create().value()} {
}

void TextWindow::draw(Context& ctx) {
	ctx.draw_filled_rect(rect, bg_color);

	uint32_t y_offset = 0;
	uint32_t x_offset = 0;

	for (auto c : text) {
		auto bitmap = glyph_cache.find(c);
		if (bitmap == glyph_cache.end()) {
			auto new_bitmap = text_ctx.rasterize_glyph(c, 8, 16, text_color, bg_color).value();
			glyph_cache.insert(std::pair<uint32_t, libtext::Bitmap> {static_cast<uint32_t>(c), std::move(new_bitmap)});
			bitmap = glyph_cache.find(c);
			assert(bitmap != glyph_cache.end());
		}

		if (x_offset + bitmap->second.width > rect.width) {
			x_offset = 0;
			y_offset += bitmap->second.height;
			if (y_offset >= rect.height) {
				return;
			}
		}

		for (uint32_t y = 0; y < bitmap->second.height; ++y) {
			for (uint32_t x = 0; x < bitmap->second.width; ++x) {
				auto color = bitmap->second[y, x];

				Rect pixel_rect = rect;
				pixel_rect.y += y_offset + y;
				pixel_rect.x += x_offset + x;
				pixel_rect.width = 1;
				pixel_rect.height = 1;
				ctx.draw_filled_rect(pixel_rect, color);
			}
		}

		x_offset += bitmap->second.width;
	}
}
