#include "ui/gui.hpp"

using namespace ui;

Gui::Gui(Context& ctx, uint32_t width, uint32_t height) : ctx {ctx} {
	auto root_window = std::make_unique<Window>(true);
	root_window->set_size(width, height);
	root_window->internal = true;
	root_windows.push_back(std::move(root_window));

	ctx.dirty_rects.push_back({
		.x = 0,
		.y = 0,
		.width = width,
		.height = height
	});
}

void Gui::draw() {
	if (ctx.dirty_rects.empty()) {
		return;
	}

	if (draw_cursor) {
		Rect mouse_rect {
			.x = mouse_state.pos.x,
			.y = mouse_state.pos.y,
			.width = 10,
			.height = 10
		};
		ctx.subtract_rects.push_back(mouse_rect);
	}

	for (auto& window : root_windows) {
		window->draw_generic(ctx, {});
	}

	ctx.x_off = 0;
	ctx.y_off = 0;

	if (draw_cursor) {
		ctx.subtract_rects.clear();
		ctx.clear_clip_rects();

		ctx.clip_rects.push_back({
			.x = 0,
			.y = 0,
			.width = ctx.width,
			.height = ctx.height
		});

		Rect mouse_rect {
			.x = mouse_state.pos.x,
			.y = mouse_state.pos.y,
			.width = 10,
			.height = 10
		};

		ctx.draw_filled_rect(mouse_rect, 0xFF0000);
	}

	ctx.dirty_rects.clear();
}

static bool handle_mouse_recursive(Gui* desktop, std::unique_ptr<Window>& window, const MouseState& new_state) {
	auto abs_offset = window->get_abs_pos_offset();

	if (!window->no_decorations) {
		if (!desktop->mouse_state.left_pressed && new_state.left_pressed) {
			Rect bar_rect {
				.x = window->rect.x + abs_offset.x,
				.y = window->rect.y + abs_offset.y,
				.width = window->titlebar->rect.width,
				.height = window->titlebar->rect.height
			};

			if (bar_rect.contains(new_state.pos)) {
				desktop->last_mouse_over = window->titlebar.get();
				if (handle_mouse_recursive(desktop, window->titlebar, new_state)) {
					desktop->dragging = nullptr;
					return true;
				}

				desktop->drag_x_off = new_state.pos.x - bar_rect.x;
				desktop->drag_y_off = new_state.pos.y - bar_rect.y;

				auto copy = std::move(window);
				desktop->dragging = &*copy;
				copy->parent->active_child = copy.get();
				copy->parent->children.erase(
					copy->parent->children.begin() +
					static_cast<std::vector<std::unique_ptr<Window>>::difference_type>(&window - copy->parent->children.data()));
				copy->parent->children.push_back(std::move(copy));
				return true;
			}
		}
	}

	for (size_t i = window->children.size(); i > 0; --i) {
		auto& child = window->children[i - 1];
		if (handle_mouse_recursive(desktop, child, new_state)) {
			if (new_state.left_pressed) {
				window->active_child = child.get();
			}
			return true;
		}
	}

	if (!window->parent) {
		return false;
	}

	if (desktop->dragging == window.get()) {
		return true;
	}

	Rect content_rect {
		.x = window->rect.x + abs_offset.x + (!window->no_decorations ? window->border.left : 0),
		.y = window->rect.y + abs_offset.y + (!window->no_decorations ? window->titlebar->rect.height : 0),
		.width = window->rect.width,
		.height = window->rect.height
	};
	if (!content_rect.contains(new_state.pos)) {
		if (!window->no_decorations) {
			Rect titlebar_rect {
				.x = window->rect.x + abs_offset.x,
				.y = window->rect.y + abs_offset.y,
				.width = window->titlebar->rect.width,
				.height = window->titlebar->rect.height
			};
			if (titlebar_rect.contains(new_state.pos)) {
				desktop->last_mouse_over = window->titlebar.get();
				if (handle_mouse_recursive(desktop, window->titlebar, new_state)) {
					return true;
				}
			}
		}

		return false;
	}

	desktop->last_mouse_over = window.get();
	return window->handle_mouse(desktop->ctx, desktop->mouse_state, new_state);
}

static bool handle_keyboard_recursive(
	Gui* desktop,
	Window* window,
	const KeyState& old_state,
	const KeyState& new_state) {

	if (window->active_child) {
		if (handle_keyboard_recursive(desktop, window->active_child, old_state, new_state)) {
			return true;
		}
	}

	return window->handle_keyboard(desktop->ctx, old_state, new_state);
}

void Gui::handle_mouse(MouseState new_state) {
	if (last_mouse_over) {
		auto window = last_mouse_over;
		auto abs_offset = window->get_abs_pos_offset();

		Rect content_rect {
			.x = window->rect.x + abs_offset.x + (!window->no_decorations ? window->border.left : 0),
			.y = window->rect.y + abs_offset.y + (!window->no_decorations ? window->titlebar->rect.height : 0),
			.width = window->rect.width,
			.height = window->rect.height
		};

		if (!content_rect.contains(new_state.pos)) {
			if (content_rect.contains(mouse_state.pos)) {
				window->on_mouse_leave(ctx, new_state);
			}
		}
		last_mouse_over = nullptr;
	}

	if (!new_state.left_pressed) {
		dragging = nullptr;
	}

	if (dragging) {
		auto old = dragging->rect;

		uint32_t new_x;
		uint32_t new_y;

		auto abs_offset = dragging->get_abs_pos_offset();
		if (new_state.pos.x >= drag_x_off) {
			if (new_state.pos.x <= abs_offset.x || (new_state.pos.x - abs_offset.x) < drag_x_off) {
				new_x = 0;
			}
			else if (new_state.pos.x - drag_x_off >=
				abs_offset.x + dragging->parent->rect.width - (dragging->rect.width + dragging->border.left + dragging->border.right)) {
				new_x = dragging->parent->rect.width - (dragging->rect.width + dragging->border.left + dragging->border.right);
			}
			else {
				new_x = (new_state.pos.x - abs_offset.x) - drag_x_off;
			}
		}
		else {
			new_x = 0;
		}

		if (new_state.pos.y >= drag_y_off) {
			if (new_state.pos.y <= abs_offset.y || (new_state.pos.y - abs_offset.y) < drag_y_off) {
				new_y = 0;
			}
			else if (new_state.pos.y - drag_y_off >=
				abs_offset.y + dragging->parent->rect.height - (dragging->rect.height + dragging->titlebar->rect.height + dragging->border.bottom)) {
				new_y = dragging->parent->rect.height - (dragging->rect.height + dragging->titlebar->rect.height + dragging->border.bottom);
			}
			else {
				new_y = (new_state.pos.y - abs_offset.y) - drag_y_off;
			}
		}
		else {
			new_y = 0;
		}

		dragging->set_pos(new_x, new_y);

		Rect abs_old {
			.x = old.x + abs_offset.x,
			.y = old.y + abs_offset.y,
			.width = old.width,
			.height = old.height
		};
		Rect abs_new {
			.x = dragging->rect.x + abs_offset.x,
			.y = dragging->rect.y + abs_offset.y,
			.width = dragging->rect.width,
			.height = dragging->rect.height
		};

		if (!dragging->no_decorations) {
			abs_old.width += dragging->border.left + dragging->border.right;
			abs_new.width += dragging->border.left + dragging->border.right;
			abs_old.height += dragging->titlebar->rect.height + dragging->border.bottom;
			abs_new.height += dragging->titlebar->rect.height + dragging->border.bottom;
		}

		if (abs_old.intersects(abs_new)) {
			auto [parts, count] = abs_old.subtract(abs_new);
			for (int i = 0; i < count; ++i) {
				ctx.dirty_rects.push_back(parts[i]);
			}
		}
		else {
			ctx.dirty_rects.push_back(abs_old);
		}
		ctx.dirty_rects.push_back(abs_new);
	}
	else {
		for (size_t i = root_windows.size(); i > 0; --i) {
			auto& window = root_windows[i - 1];
			if (handle_mouse_recursive(this, window, new_state)) {
				break;
			}
		}
	}

	if (draw_cursor) {
		Rect mouse_rect {
			.x = mouse_state.pos.x,
			.y = mouse_state.pos.y,
			.width = std::min(10U, ctx.width - mouse_state.pos.x),
			.height = std::min(10U, ctx.height - mouse_state.pos.y)
		};

		Rect new_mouse_rect {
			.x = new_state.pos.x,
			.y = new_state.pos.y,
			.width = std::min(10U, ctx.width - new_state.pos.x),
			.height = std::min(10U, ctx.height - new_state.pos.y)
		};
		if (mouse_rect.intersects(new_mouse_rect)) {
			auto [parts, count] = mouse_rect.subtract(new_mouse_rect);
			for (int i = 0; i < count; ++i) {
				ctx.dirty_rects.push_back(parts[i]);
			}
		}
		else {
			ctx.dirty_rects.push_back(mouse_rect);
		}
		ctx.dirty_rects.push_back(new_mouse_rect);
	}

	mouse_state = new_state;
}

void Gui::handle_keyboard(KeyState new_state) {
	KeyState old_state {
		.code = new_state.code,
		.pressed = key_states[new_state.code]
	};

	for (size_t i = root_windows.size(); i > 0; --i) {
		auto& window = root_windows[i - 1];
		if (handle_keyboard_recursive(this, window.get(), old_state, new_state)) {
			break;
		}
	}

	key_states[new_state.code] = new_state.pressed;
}

void Gui::destroy_window(Window* window) {
	if (window->parent->active_child == window) {
		window->parent->active_child = nullptr;
	}

	dragging = nullptr;

	if (last_mouse_over == window ||
		last_mouse_over == window->titlebar.get()) {
		last_mouse_over = nullptr;
	}
	else {
		std::vector<Window*> stack;
		stack.push_back(window);
		if (!window->no_decorations) {
			stack.push_back(window->titlebar.get());
		}

		while (!stack.empty()) {
			auto entry = stack.back();
			stack.pop_back();
			if (last_mouse_over == entry) {
				last_mouse_over = nullptr;
			}

			for (auto& child : entry->children) {
				stack.push_back(child.get());
			}
		}
	}

	for (size_t i = 0; i < window->parent->children.size(); ++i) {
		if (window->parent->children[i].get() == window) {
			auto dirty_rect = window->get_abs_rect();
			if (!window->no_decorations) {
				dirty_rect.width += window->border.left + window->border.right;
				dirty_rect.height += window->titlebar->rect.height + window->border.bottom;
			}
			ctx.dirty_rects.push_back(dirty_rect);

			window->parent->children.erase(
				window->parent->children.begin() +
				static_cast<std::vector<std::unique_ptr<Window>>::difference_type>(i));
			break;
		}
	}
}
