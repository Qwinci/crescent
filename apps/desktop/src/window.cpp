#include "window.hpp"
#include "event.hpp"
#include <ui/button.hpp>
#include <ui/text.hpp>

struct TitlebarWindow : public ui::Window {
	explicit TitlebarWindow(Window* parent) : Window {true} {
		this->parent = parent;
		is_titlebar = true;
		rect.height = TITLEBAR_HEIGHT;
		bg_color = TITLEBAR_ACTIVE_COLOR;

		auto close_button_unique = std::make_unique<ui::ButtonWindow>();
		close_button_unique->set_size(TITLEBAR_HEIGHT - BORDER_WIDTH / 2, TITLEBAR_HEIGHT - BORDER_WIDTH / 2);

		close_button = close_button_unique.get();
		add_child(std::move(close_button_unique));

		auto title_unique = std::make_unique<ui::TextWindow>();
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

	bool handle_mouse(ui::Context&, const ui::MouseState&, const ui::MouseState&) override {
		return false;
	}

	void on_mouse_leave(ui::Context&, const ui::MouseState&) override {}

	void on_mouse_enter(ui::Context&, const ui::MouseState&) override {}

	bool handle_keyboard(ui::Context&, const ui::KeyState&, const ui::KeyState&) override {
		return false;
	}

	ui::TextWindow* title;
	ui::ButtonWindow* close_button;
};

DesktopWindow::DesktopWindow(bool no_decorations) : Window {no_decorations} {
	if (!no_decorations) {
		titlebar = std::make_unique<TitlebarWindow>(this);
		auto close_button = static_cast<TitlebarWindow*>(titlebar.get())->close_button;
		close_button->callback = [](void* arg) {
			auto* window = static_cast<Window*>(arg);
			if (window->internal) {
				if (window->on_close) {
					window->on_close(window, window->on_close_arg);
				}
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

		border.left = BORDER_WIDTH;
		border.right = BORDER_WIDTH;
		border.bottom = BORDER_WIDTH;

		border_color = BORDER_COLOR;
	}
}

bool DesktopWindow::handle_mouse(ui::Context&, const ui::MouseState&, const ui::MouseState& new_state) {
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

void DesktopWindow::on_mouse_enter(ui::Context&, const ui::MouseState& new_state) {
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

void DesktopWindow::on_mouse_leave(ui::Context&, const ui::MouseState& new_state) {
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

bool DesktopWindow::handle_keyboard(ui::Context&, const ui::KeyState& old_state, const ui::KeyState& new_state) {
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

void DesktopWindow::set_title(std::string_view title) {
	if (no_decorations) {
		return;
	}
	static_cast<TitlebarWindow*>(titlebar.get())->title->text = title;
}
