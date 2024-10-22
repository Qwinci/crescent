#include "context.hpp"
#include <algorithm>
#include <cassert>

void Context::draw_filled_rect(const Rect& rect, uint32_t color) const {
	auto rect_x = rect.x + x_off;
	auto rect_y = rect.y + y_off;

	for (auto& clip_rect : clip_rects) {
		for (auto& other : clip_rects) {
			if (&clip_rect == &other) {
				continue;
			}
			assert(!clip_rect.intersects(other));
		}
	}

	for (auto& clip_rect : clip_rects) {
		if (rect_x + rect.width <= clip_rect.x || rect_y >= clip_rect.y + clip_rect.height) {
			continue;
		}

		auto start_x = std::max(rect_x, clip_rect.x);
		auto res_width = std::min(rect_x + rect.width, clip_rect.x + clip_rect.width) - start_x;
		auto start_y = std::max(rect_y, clip_rect.y);
		auto res_height = std::min(rect_y + rect.height, clip_rect.y + clip_rect.height) - start_y;

		for (uint32_t y = start_y; y < start_y + res_height; ++y) {
			for (uint32_t x = start_x; x < start_x + res_width; ++x) {
				//assert(fb[y * pitch_32 + x] == 0xCFCFCFCF);
				fb[y * pitch_32 + x] = color;
			}
		}
	}
}

void Context::draw_rect_outline(const Rect& rect, uint32_t color, uint32_t thickness) const {
	draw_filled_rect({
		.x = rect.x,
		.y = rect.y,
		.width = rect.width,
		.height = thickness
	 }, color);
	draw_filled_rect({
		.x = rect.x,
		.y = rect.y + rect.height - thickness,
		.width = rect.width,
		.height = thickness
	}, color);
	draw_filled_rect({
		.x = rect.x,
		.y = rect.y,
		.width = thickness,
		.height = rect.height
	}, color);
	draw_filled_rect({
		.x = rect.x + rect.width - thickness,
		.y = rect.y,
		.width = thickness,
		.height = rect.height
	}, color);
}

void Context::add_clip_rect(const Rect& rect) {
	std::vector<Rect> res;
	res.push_back(rect);

	for (auto& subtract : subtract_rects) {
		for (size_t i = 0; i < res.size();) {
			if (res[i].intersects(subtract)) {
				auto [splits, count] = res[i].subtract(subtract);
				res.erase(
					res.begin() +
					static_cast<ptrdiff_t>(i));
				for (int j = 0; j < count; ++j) {
					res.push_back(splits[j]);
				}
				continue;
			}

			++i;
		}
	}

	for (auto& res_rect : res) {
		clip_rects.push_back(res_rect);
	}
}

void Context::clear_clip_rects() {
	clip_rects.clear();
}