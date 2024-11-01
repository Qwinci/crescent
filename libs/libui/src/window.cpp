#include "ui/window.hpp"
#include <cstring>

using namespace ui;

Window::Window(bool no_decorations) : no_decorations {no_decorations} {}

void Window::draw(Context& ctx) {
	ctx.draw_filled_rect(rect, bg_color);
}

static void draw_decorations_generic(Context& ctx, Window* window) {
	window->draw(ctx);

	for (auto& child : window->children) {
		draw_decorations_generic(ctx, child.get());
	}
}

void Window::draw_decorations(Context& ctx) {
	Rect left_border_rect {
		.x = 0,
		.y = titlebar->rect.height,
		.width = border.left,
		.height = rect.height
	};
	Rect right_border_rect {
		.x = border.left + rect.width,
		.y = titlebar->rect.height,
		.width = border.right,
		.height = rect.height
	};
	Rect bottom_border_rect {
		.x = 0,
		.y = titlebar->rect.height + rect.height,
		.width = border.left + rect.width + border.right,
		.height = border.bottom
	};

	draw_decorations_generic(ctx, titlebar.get());

	ctx.draw_filled_rect(left_border_rect, border_color);
	ctx.draw_filled_rect(bottom_border_rect, border_color);
	ctx.draw_filled_rect(right_border_rect, border_color);
}

void Window::draw_generic(Context& ctx, std::vector<Rect> parent_clip_rects) {
	// --------------------------- clipping ---------------------------
	ctx.clear_clip_rects();

	std::vector<Rect> clip_rects = std::move(parent_clip_rects);
	if (parent) {
		// subtract sibling rects above the current window
		size_t index = 0;
		for (; index < parent->children.size(); ++index) {
			if (parent->children[index].get() == this) {
				break;
			}
		}
		++index;

		for (; index < parent->children.size(); ++index) {
			auto& sibling = parent->children[index];
			auto sibling_rect = sibling->get_abs_rect();
			if (!sibling->no_decorations) {
				sibling_rect.width += sibling->border.left + sibling->border.right;
				sibling_rect.height += sibling->titlebar->rect.height + sibling->border.bottom;
			}

			for (size_t i = 0; i < clip_rects.size();) {
				auto& old_rect = clip_rects[i];
				if (old_rect.intersects(sibling_rect)) {
					auto [splits, count] = old_rect.subtract(sibling_rect);
					clip_rects.erase(
						clip_rects.begin() +
						static_cast<ptrdiff_t>(i));
					for (int j = 0; j < count; ++j) {
						clip_rects.push_back(splits[j]);
					}
					continue;
				}

				++i;
			}
		}
	}
	else {
		// root window, add viewport rect
		//clip_rects.push_back(get_abs_rect());

		// root window, add dirty rects
		for (auto& dirty_rect : ctx.dirty_rects) {
			for (size_t i = 0; i < clip_rects.size();) {
				auto& old_rect = clip_rects[i];
				if (old_rect.intersects(dirty_rect)) {
					auto [splits, count] = old_rect.subtract(dirty_rect);
					clip_rects.erase(
						clip_rects.begin() +
						static_cast<ptrdiff_t>(i));
					for (int j = 0; j < count; ++j) {
						clip_rects.push_back(splits[j]);
					}
					continue;
				}

				++i;
			}

			clip_rects.push_back(dirty_rect);
		}
	}

	auto new_parent_rects = clip_rects;

	// subtract child rects
	for (auto& child : children) {
		auto child_rect = child->get_abs_rect();
		if (!child->no_decorations) {
			child_rect.width += child->border.left + child->border.right;
			child_rect.height += child->titlebar->rect.height + child->border.bottom;
		}

		for (size_t i = 0; i < clip_rects.size();) {
			auto& old_rect = clip_rects[i];
			if (old_rect.intersects(child_rect)) {
				auto [splits, count] = old_rect.subtract(child_rect);
				clip_rects.erase(
					clip_rects.begin() +
					static_cast<ptrdiff_t>(i));
				for (int j = 0; j < count; ++j) {
					clip_rects.push_back(splits[j]);
				}
				continue;
			}

			++i;
		}
	}

	for (auto& clip_rect : clip_rects) {
		ctx.add_clip_rect(clip_rect);
	}

	// --------------------------- clipping end ---------------------------

	auto pos = get_abs_pos_offset();

	if (!no_decorations) {
		ctx.y_off = pos.y + rect.y;
		ctx.x_off = pos.x + rect.x;

		draw_decorations(ctx);

		ctx.y_off = pos.y + titlebar->rect.height;
		ctx.x_off = pos.x + border.left;
	}
	else {
		ctx.y_off = pos.y;
		ctx.x_off = pos.x;
	}

	if (fb) {
		Rect content_rect {
			.x = ctx.x_off + rect.x,
			.y = ctx.y_off + rect.y,
			.width = rect.width,
			.height = rect.height
		};

		for (auto& clip_rect : ctx.clip_rects) {
			if (!clip_rect.intersects(content_rect)) {
				continue;
			}

			auto clipped = content_rect.intersect(clip_rect);

			uint32_t rel_x = clipped.x - content_rect.x;

			for (uint32_t y = clipped.y; y < clipped.y + clipped.height; ++y) {
				uint32_t rel_y = y - content_rect.y;
				size_t to_copy = clipped.width * 4;

				auto dest_ptr = &ctx.fb[y * ctx.pitch_32 + clipped.x];
				auto src_ptr = &fb[rel_y * rect.width + rel_x];
				for (size_t i = 0; i < clipped.width; ++i) {
					//assert(dest_ptr[i] == 0xCFCFCFCF);
				}
				memcpy(dest_ptr, src_ptr, to_copy);
			}
		}
	}
	else {
		draw(ctx);
	}

	for (auto& child : children) {
		child->draw_generic(ctx, new_parent_rects);
	}
}

void Window::add_child(std::unique_ptr<Window> child) {
	child->parent = this;
	children.push_back(std::move(child));
}
