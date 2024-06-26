#include "window.hpp"
#include <string.h>

void Window::draw(Context& ctx) {
	if (!parent) {
		ctx.draw_filled_rect(rect, bg_color);
	}
	else {
		ctx.draw_filled_rect(rect, bg_color);
	}
}

bool Window::handle_mouse(Context&, const MouseState&) {
	return true;
}

void Window::on_mouse_enter(Context&, const MouseState&) {}

void Window::on_mouse_leave(Context&, const MouseState&) {}

void Window::draw_generic(Context& ctx) {
	auto abs_rect = get_abs_rect();
	if (!no_decorations) {
		abs_rect.width += BORDER_WIDTH * 2;
		abs_rect.height += TITLEBAR_HEIGHT + BORDER_WIDTH;
	}

	std::vector<Rect> clip_rects;
	for (auto& dirty_rect : ctx.dirty_rects) {
		if (!dirty_rect.intersects(abs_rect)) {
			continue;
		}
		clip_rects.push_back(dirty_rect.intersect(abs_rect));
	}
	if (clip_rects.empty()) {
		return;
	}

	for (auto& child : children) {
		Rect abs_child_rect = child->get_abs_rect();
		if (!child->no_decorations) {
			abs_child_rect.width += BORDER_WIDTH * 2;
			abs_child_rect.height += TITLEBAR_HEIGHT + BORDER_WIDTH;
		}

		for (size_t i = 0; i < clip_rects.size();) {
			auto& clip_rect = clip_rects[i];
			if (!clip_rect.intersects(abs_child_rect)) {
				++i;
				continue;
			}
			auto [parts, count] = clip_rect.subtract(abs_child_rect);
			clip_rects.erase(
				clip_rects.begin() +
				static_cast<std::vector<Rect>::difference_type>(i)
			);
			for (int j = 0; j < count; ++j) {
				clip_rects.push_back(parts[j]);
			}
		}
	}
	if (parent) {
		size_t this_index = 0;
		while (parent->children[this_index].get() != this) {
			++this_index;
		}

		for (size_t sibling_i = this_index + 1; sibling_i < parent->children.size(); ++sibling_i) {
			auto& sibling = parent->children[sibling_i];

			Rect abs_sibling_rect = sibling->get_abs_rect();
			if (!sibling->no_decorations) {
				abs_sibling_rect.width += BORDER_WIDTH * 2;
				abs_sibling_rect.height += TITLEBAR_HEIGHT + BORDER_WIDTH;
			}

			for (size_t i = 0; i < clip_rects.size();) {
				auto& clip_rect = clip_rects[i];
				if (!clip_rect.intersects(abs_sibling_rect)) {
					++i;
					continue;
				}
				auto [parts, count] = clip_rect.subtract(abs_sibling_rect);
				clip_rects.erase(
					clip_rects.begin() +
					static_cast<std::vector<Rect>::difference_type>(i)
					);
				for (int j = 0; j < count; ++j) {
					clip_rects.push_back(parts[j]);
				}
			}
		}
	}

	auto abs_offset = get_abs_pos_offset();

	ctx.x_off = abs_offset.x;
	ctx.y_off = abs_offset.y;

	if (!no_decorations) {
		Rect titlebar_rect {
			.x = rect.x,
			.y = rect.y,
			.width = abs_rect.width,
			.height = TITLEBAR_HEIGHT
		};

		Rect left_border_rect {
			.x = rect.x,
			.y = rect.y + TITLEBAR_HEIGHT,
			.width = BORDER_WIDTH,
			.height = rect.height
		};
		Rect right_border_rect {
			.x = rect.x + BORDER_WIDTH + rect.width,
			.y = rect.y + TITLEBAR_HEIGHT,
			.width = BORDER_WIDTH,
			.height = rect.height
		};
		Rect bottom_border_rect {
			.x = rect.x,
			.y = rect.y + TITLEBAR_HEIGHT + rect.height,
			.width = rect.width + BORDER_WIDTH * 2,
			.height = BORDER_WIDTH
		};

		for (auto& clip_rect : clip_rects) {
			ctx.set_clip_rect(clip_rect);
			ctx.draw_filled_rect(titlebar_rect, TITLEBAR_ACTIVE_COLOR);
			ctx.draw_filled_rect(left_border_rect, BORDER_COLOR);
			ctx.draw_filled_rect(right_border_rect, BORDER_COLOR);
			ctx.draw_filled_rect(bottom_border_rect, BORDER_COLOR);
		}

		ctx.y_off += TITLEBAR_HEIGHT;
		ctx.x_off += BORDER_WIDTH;
	}

	Rect ctx_rect {
		.x = 0,
		.y = 0,
		.width = ctx.width,
		.height = ctx.height
	};

	for (auto& clip_rect : clip_rects) {
		ctx.set_clip_rect(clip_rect);
		draw(ctx);

		if (fb && clip_rect.intersects(ctx_rect)) {
			auto res_rect = clip_rect.intersect(ctx_rect);

			auto rect_x = rect.x + ctx.x_off;
			auto rect_y = rect.y + ctx.y_off;

			Rect content_rect {
				.x = rect_x,
				.y = rect_y,
				.width = rect.width,
				.height = rect.height
			};

			if (!res_rect.intersects(content_rect)) {
				continue;
			}

			res_rect = res_rect.intersect(content_rect);

			auto abs_pos_offset = get_abs_pos_offset();

			uint32_t rel_x = res_rect.x - abs_pos_offset.x - rect.x;
			if (!no_decorations) {
				rel_x -= BORDER_WIDTH;
			}

			for (uint32_t y = res_rect.y; y < res_rect.y + res_rect.height; ++y) {
				uint32_t rel_y = y - abs_pos_offset.y - rect.y;
				if (!no_decorations) {
					rel_y -= TITLEBAR_HEIGHT;
				}

				size_t to_copy = std::min(res_rect.width, rect.width) * 4;
				memcpy(&ctx.fb[y * ctx.pitch_32 + res_rect.x], &fb[rel_y * rect.width + rel_x], to_copy);
			}
		}
	}

	for (auto& child : children) {
		child->draw_generic(ctx);
	}
}

void Window::add_child(std::unique_ptr<Window> child) {
	child->parent = this;
	children.push_back(std::move(child));
}
