#pragma once
#include <cstdint>
#include <vector>
#include <cassert>
#include "primitive.hpp"

namespace ui {
	struct Context {
		uint32_t* fb {};
		uintptr_t pitch_32 {};
		uint32_t width {};
		uint32_t height {};
		uint32_t x_off {};
		uint32_t y_off {};
		std::vector<Rect> dirty_rects {};

		void add_clip_rect(const Rect& rect);
		void clear_clip_rects();

		void draw_filled_rect(const Rect& rect, uint32_t color) const;
		void draw_bitmap(
			const uint32_t* pixels,
			uint32_t x,
			uint32_t y,
			uint32_t width,
			uint32_t height) const;
		void draw_rect_outline(const Rect& rect, uint32_t color, uint32_t thickness) const;

		std::vector<Rect> clip_rects {};
		std::vector<Rect> subtract_rects {};
	};
}
