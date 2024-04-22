#include "context.hpp"
#include <algorithm>

void Context::set_clip_rect(const Rect& rect) {
	Rect ctx_rect {
		.x = 0,
		.y = 0,
		.width = width,
		.height = height
	};
	if (!rect.intersects(ctx_rect)) {
		clip_rect = {.x = 0, .y = 0, .width = 0, .height = 0};
		return;
	}
	clip_rect = rect.intersect(ctx_rect);
}

void Context::draw_filled_rect(const Rect& rect, uint32_t color) const {
	auto rect_x = rect.x + x_off;
	auto rect_y = rect.y + y_off;

	if (rect_x + rect.width <= clip_rect.x || rect_y >= clip_rect.y + clip_rect.height) {
		return;
	}

	auto start_x = std::min(std::max(rect_x, clip_rect.x), width);
	auto res_width = std::min(rect_x + rect.width - start_x, clip_rect.x + clip_rect.width - start_x);
	auto start_y = std::min(std::max(rect_y, clip_rect.y), height);
	auto res_height = std::min(rect_y + rect.height - start_y, clip_rect.y + clip_rect.height - start_y);

	for (uint32_t y = start_y; y < start_y + res_height; ++y) {
		for (uint32_t x = start_x; x < start_x + res_width; ++x) {
			fb[y * pitch_32 + x] = color;
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
