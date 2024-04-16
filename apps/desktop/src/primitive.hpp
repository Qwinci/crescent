#pragma once
#include <cstdint>
#include <algorithm>
#include <array>
#include <utility>

struct Point {
	uint32_t x;
	uint32_t y;
};

struct Rect {
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;

	[[nodiscard]] constexpr bool contains(const Point& point) const {
		return point.x >= x && point.x < x + width &&
		       point.y >= y && point.y < y + height;
	}

	[[nodiscard]] constexpr bool intersects(const Rect& other) const {
		return x < other.x + other.width && other.x < x + width &&
			y < other.y + other.height && other.y < y + height;
	}

	[[nodiscard]] constexpr Rect intersect(const Rect& other) const {
		auto start_x = std::max(x, other.x);
		auto res_width = std::min(x + width - start_x, other.x + other.width - start_x);
		auto start_y = std::max(y, other.y);
		auto res_height = std::min(y + height - start_y, other.y + other.height - start_y);
		return {.x = start_x, .y = start_y, .width = res_width, .height = res_height};
	}

	[[nodiscard]] constexpr std::pair<std::array<Rect, 4>, int> subtract(const Rect& other) const {
		std::array<Rect, 4> res {};
		int count = 0;

		auto right_x = x + width;
		auto bottom_y = y + height;
		auto other_right_x = other.x + other.width;
		auto other_bottom_y = other.y + other.height;

		if (other.x > x) {
			auto x_overlap = right_x - other.x;
			Rect left {
				.x = x,
				.y = y,
				.width = width - x_overlap,
				.height = height
			};
			res[count++] = left;
		}
		if (other_right_x < right_x) {
			Rect right {
				.x = other_right_x,
				.y = y,
				.width = right_x - other_right_x,
				.height = height
			};
			res[count++] = right;
		}
		if (other.y > y) {
			Rect top {
				.x = x,
				.y = y,
				.width = width,
				.height = other.y - y
			};
			res[count++] = top;
		}
		if (other_bottom_y < bottom_y) {
			Rect bottom {
				.x = x,
				.y = other_bottom_y,
				.width = width,
				.height = bottom_y - other_bottom_y
			};
			res[count++] = bottom;
		}

		return {res, count};
	}

	constexpr bool operator==(const Rect& other) const {
		return x == other.x && y == other.y && width == other.width && height == other.height;
	}
};
