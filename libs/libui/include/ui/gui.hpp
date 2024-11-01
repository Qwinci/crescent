#pragma once
#include "window.hpp"
#include "mouse.hpp"

namespace ui {
	struct Gui {
		explicit Gui(Context& ctx, uint32_t width, uint32_t height);

		void draw();
		void handle_mouse(MouseState new_state);
		void handle_keyboard(KeyState new_state);

		void destroy_window(Window* window);

		std::vector<std::unique_ptr<Window>> root_windows;
		Context& ctx;
		MouseState mouse_state {};
		uint32_t drag_x_off {};
		uint32_t drag_y_off {};
		Window* dragging {};
		Window* last_mouse_over {};
		bool key_states[SCANCODE_MAX] {};
		bool draw_cursor {true};
	};
}
