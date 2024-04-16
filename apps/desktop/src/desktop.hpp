#pragma once
#include "window.hpp"
#include "mouse.hpp"

struct Desktop {
	explicit Desktop(Context& ctx);

	void draw();
	void handle_mouse(MouseState new_state);

	std::unique_ptr<Window> root_window;
	Context& ctx;
	MouseState mouse_state {};
	uint32_t drag_x_off {};
	uint32_t drag_y_off {};
	Window* dragging {};
};
