#pragma once
#include <string_view>
#include <cstdint>
#include <optional>
#include <vector>

namespace libtext {
	struct Bitmap {
		std::vector<uint32_t> pixels;
		uint32_t width;
		uint32_t height;

		[[nodiscard]] constexpr uint32_t operator[](size_t y, size_t x) const {
			return pixels[y * width + x];
		}
	};

	struct Context {
		static std::optional<Context> create(std::string_view font_path = "");

		std::optional<Bitmap> rasterize_glyph(uint32_t codepoint, uint32_t width, uint32_t height, uint32_t fg, uint32_t bg);

	private:
		constexpr explicit Context(std::vector<uint8_t> font, std::vector<uint16_t> glyph_mappings)
			: font {std::move(font)}, glyph_mappings {std::move(glyph_mappings)} {}

		std::vector<uint8_t> font;
		std::vector<uint16_t> glyph_mappings;
	};
}
