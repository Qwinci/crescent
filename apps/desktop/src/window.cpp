#include "window.hpp"
#include "button.hpp"
#include "event.hpp"
#include "text.hpp"
#include <cstring>
#include <cassert>

struct TitlebarWindow : public Window {
	explicit TitlebarWindow(Window* parent) : Window {true} {
		this->parent = parent;
		is_titlebar = true;
		rect.height = TITLEBAR_HEIGHT;
		bg_color = TITLEBAR_ACTIVE_COLOR;

		auto close_button_unique = std::make_unique<ButtonWindow>();
		close_button_unique->set_size(TITLEBAR_HEIGHT - BORDER_WIDTH / 2, TITLEBAR_HEIGHT - BORDER_WIDTH / 2);

		close_button = close_button_unique.get();
		add_child(std::move(close_button_unique));

		auto title_unique = std::make_unique<TextWindow>();
		title_unique->text_color = 0;
		title_unique->bg_color = TITLEBAR_ACTIVE_COLOR;
		title_unique->set_size(rect.width / 2, TITLEBAR_HEIGHT - BORDER_WIDTH / 2);

		title = title_unique.get();
		add_child(std::move(title_unique));
	}

	void update_titlebar(uint32_t width) override {
		rect.width = width + BORDER_WIDTH * 2;
		close_button->set_pos(rect.width - TITLEBAR_HEIGHT - BORDER_WIDTH, BORDER_WIDTH / 4);
		title->set_size(rect.width / 2, TITLEBAR_HEIGHT - BORDER_WIDTH / 2);
	}

	bool handle_mouse(Context& ctx, const MouseState& old_state, const MouseState& new_state) override {
		return false;
	}

	void on_mouse_leave(Context&, const MouseState&) override {}

	void on_mouse_enter(Context&, const MouseState&) override {}

	bool handle_keyboard(Context&, const KeyState&, const KeyState&) override {
		return false;
	}

	TextWindow* title;
	ButtonWindow* close_button;
};

Window::Window(bool no_decorations) : no_decorations {no_decorations} {
	if (!no_decorations) {
		titlebar = std::make_unique<TitlebarWindow>(this);
		auto close_button = static_cast<TitlebarWindow*>(titlebar.get())->close_button;
		close_button->callback = [](void* arg) {
			auto* window = static_cast<Window*>(arg);
			if (window->internal) {
				return;
			}

			protocol::WindowEvent event {
				.type = protocol::WindowEvent::CloseRequested,
				.window_handle = window,
				.dummy {}
			};
			send_event_to_window(window, event);
		};
		close_button->arg = this;
	}
}

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
		.y = TITLEBAR_HEIGHT,
		.width = BORDER_WIDTH,
		.height = rect.height + BORDER_WIDTH
	};
	Rect right_border_rect {
		.x = rect.width + BORDER_WIDTH,
		.y = TITLEBAR_HEIGHT,
		.width = BORDER_WIDTH,
		.height = rect.height
	};
	Rect bottom_border_rect {
		.x = BORDER_WIDTH,
		.y = rect.height + TITLEBAR_HEIGHT,
		.width = rect.width + BORDER_WIDTH,
		.height = BORDER_WIDTH
	};

	draw_decorations_generic(ctx, titlebar.get());

	ctx.draw_filled_rect(left_border_rect, BORDER_COLOR);
	ctx.draw_filled_rect(bottom_border_rect, BORDER_COLOR);
	ctx.draw_filled_rect(right_border_rect, BORDER_COLOR);
}

bool Window::handle_mouse(Context&, const MouseState&, const MouseState& new_state) {
	if (internal) {
		return true;
	}

	auto offset = get_abs_pos_offset();
	// todo make window handle non-opaque
	protocol::WindowEvent event {
		.type = protocol::WindowEvent::Mouse,
		.window_handle = this,
		.mouse {
			.x = new_state.pos.x - offset.x,
			.y = new_state.pos.y - offset.y,
			.left_pressed = new_state.left_pressed,
			.right_pressed = new_state.right_pressed,
			.middle_pressed = new_state.middle_pressed
		}
	};
	send_event_to_window(this, event);
	return true;
}

void Window::on_mouse_enter(Context&, const MouseState& new_state) {
	if (internal) {
		return;
	}

	auto offset = get_abs_pos_offset();
	// todo make window handle non-opaque
	protocol::WindowEvent event {
		.type = protocol::WindowEvent::MouseEnter,
		.window_handle = this,
		.mouse {
			.x = new_state.pos.x - offset.x,
			.y = new_state.pos.y - offset.y,
			.left_pressed = new_state.left_pressed,
			.right_pressed = new_state.right_pressed,
			.middle_pressed = new_state.middle_pressed
		}
	};
	send_event_to_window(this, event);
}

void Window::on_mouse_leave(Context&, const MouseState& new_state) {
	if (internal) {
		return;
	}

	auto offset = get_abs_pos_offset();
	// todo make window handle non-opaque
	protocol::WindowEvent event {
		.type = protocol::WindowEvent::MouseLeave,
		.window_handle = this,
		.mouse {
			.x = new_state.pos.x - offset.x,
			.y = new_state.pos.y - offset.y,
			.left_pressed = new_state.left_pressed,
			.right_pressed = new_state.right_pressed,
			.middle_pressed = new_state.middle_pressed
		}
	};
	send_event_to_window(this, event);
}

bool Window::handle_keyboard(Context&, const KeyState& old_state, const KeyState& new_state) {
	if (internal) {
		return true;
	}

	// todo make window handle non-opaque
	protocol::WindowEvent event {
		.type = protocol::WindowEvent::Key,
		.window_handle = this,
		.key {
			.code = new_state.code,
			.prev_pressed = old_state.pressed,
			.pressed = new_state.pressed
		}
	};
	send_event_to_window(this, event);
	return true;
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
				sibling_rect.width += BORDER_WIDTH * 2;
				sibling_rect.height += TITLEBAR_HEIGHT + BORDER_WIDTH;
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
			child_rect.width += BORDER_WIDTH * 2;
			child_rect.height += TITLEBAR_HEIGHT + BORDER_WIDTH;
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

		ctx.y_off = pos.y + TITLEBAR_HEIGHT;
		ctx.x_off = pos.x + BORDER_WIDTH;
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

void Window::set_title(std::string_view title) {
	if (no_decorations) {
		return;
	}
	static_cast<TitlebarWindow*>(titlebar.get())->title->text = title;
}
