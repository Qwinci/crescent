#include "window.hpp"

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
		abs_rect.height += TITLEBAR_HEIGHT;
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
			abs_child_rect.height += TITLEBAR_HEIGHT;
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
				abs_sibling_rect.height += TITLEBAR_HEIGHT;
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

	Rect titlebar_rect {
		.x = rect.x,
		.y = rect.y,
		.width = rect.width,
		.height = TITLEBAR_HEIGHT
	};

	if (!no_decorations) {
		for (auto& clip_rect : clip_rects) {
			ctx.clip_rect = clip_rect;
			ctx.draw_filled_rect(titlebar_rect, TITLEBAR_ACTIVE_COLOR);
		}

		ctx.y_off += TITLEBAR_HEIGHT;
	}

	for (auto& clip_rect : clip_rects) {
		ctx.clip_rect = clip_rect;
		draw(ctx);
	}

	for (auto& child : children) {
		child->draw_generic(ctx);
	}
}

void Window::add_child(std::unique_ptr<Window> child) {
	child->parent = this;
	children.push_back(std::move(child));
}
