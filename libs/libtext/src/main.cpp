#include "libtext/libtext.hpp"
#include "sys.h"
#include <cassert>

using namespace libtext;

struct Psf2Header {
	uint32_t magic;
	uint32_t version;
	uint32_t header_size;
	uint32_t flags;
	uint32_t num_glyph;
	uint32_t bytes_per_glyph;
	uint32_t height;
	uint32_t width;
};

std::optional<Context> Context::create(std::string_view font_path) {
	if (font_path.empty()) {
		font_path = "/usr/crescent/Tamsyn8x16r.psf";
	}

	CrescentHandle handle;
	if (sys_open(&handle, font_path.data(), font_path.size(), 0) != 0) {
		assert(!"failed to open /usr/crescent/Tamsyn8x16r.psf");
		return std::nullopt;
	}

	CrescentStat stat {};
	if (sys_stat(handle, &stat) != 0) {
		auto status = sys_close_handle(handle);
		assert(status == 0);
		return std::nullopt;
	}

	std::vector<uint8_t> data;
	data.resize(stat.size);
	auto status = sys_read(handle, data.data(), data.size(), nullptr);
	assert(status == 0);
	status = sys_close_handle(handle);
	assert(status == 0);

	auto* hdr = std::launder(reinterpret_cast<Psf2Header*>(data.data()));

	std::vector<uint16_t> glyph_mappings;

	if (hdr->flags) {
		glyph_mappings.resize(UINT16_MAX);
		uint16_t glyph = 0;
		for (size_t i = hdr->header_size + hdr->num_glyph * hdr->bytes_per_glyph; i < stat.size; ++i) {
			auto byte = data[i];

			uint32_t cp;

			if (byte == 0xFF) {
				++glyph;
				continue;
			}
			else if (byte & 1 << 7) {
				if ((byte & 0b11100000) == 0b11000000) {
					assert(i + 1 < stat.size);
					auto second = data[i + 1];
					++i;
					assert((second & 0b11000000) == 0b10000000);
					cp = (second & 0x3F) | (byte & 0b11111) << 6;
				}
				else if ((byte & 0b11110000) == 0b11100000) {
					assert(i + 2 < stat.size);
					auto second = data[i + 1];
					auto third = data[i + 2];
					i += 2;
					assert((second & 0b11000000) == 0b10000000);
					assert((third & 0b11000000) == 0b10000000);
					cp = (third & 0x3F) | (second & 0x3F) << 6 | (byte & 0b1111) << 12;
				}
				else {
					assert((byte & 0b11111000) == 0b11110000);
					assert(i + 3 < stat.size);
					auto second = data[i + 1];
					auto third = data[i + 2];
					auto fourth = data[i + 2];
					i += 3;
					assert((second & 0b11000000) == 0b10000000);
					assert((third & 0b11000000) == 0b10000000);
					assert((fourth & 0b11000000) == 0b10000000);
					cp =
						(fourth & 0x3F) |
						(third & 0x3F) << 6 |
						(second & 0x3F) << 12 |
						(byte & 0b111) << 18;
				}
			}
			else {
				cp = byte;
			}

			assert(cp < UINT16_MAX);
			glyph_mappings[cp] = glyph;
		}
	}

	return Context {std::move(data), std::move(glyph_mappings)};
}

std::optional<Bitmap> Context::rasterize_glyph(uint32_t codepoint, uint32_t width, uint32_t height, uint32_t fg, uint32_t bg) {
	assert(!font.empty());

	auto* hdr = std::launder(reinterpret_cast<Psf2Header*>(font.data()));
	assert(hdr);

	uint16_t glyph = 0;
	if (!glyph_mappings.empty()) {
		assert(codepoint < glyph_mappings.size());
		glyph = glyph_mappings[codepoint];
	}
	else {
		assert(codepoint < UINT16_MAX);
		glyph = codepoint;
	}

	uint32_t bytes_per_line = (hdr->width + 7) / 8;

	Bitmap bitmap {};
	bitmap.width = width;
	bitmap.height = height;
	bitmap.pixels.resize(width * height);

	assert(glyph < hdr->num_glyph);
	auto* glyph_data = font.data() + hdr->header_size + glyph * hdr->bytes_per_glyph;
	for (uint32_t y = 0; y < hdr->height; ++y) {
		for (uint32_t x = 0; x < hdr->width; ++x) {
			uint32_t shift = hdr->width - 1 - x;
			uint32_t color = (glyph_data[shift / 8] & (1 << (shift % 8))) ? fg : bg;
			bitmap.pixels[y * width + x] = color;
		}

		glyph_data += bytes_per_line;
	}

	return bitmap;
}
