#pragma once
#include "window.hpp"
#include "mouse.hpp"
#include "taskbar.hpp"

struct Desktop {
	explicit Desktop(Context& ctx);

	void draw();
	void handle_mouse(MouseState new_state);
	void handle_keyboard(KeyState new_state);

	std::unique_ptr<Window> root_window;
	std::unique_ptr<Window> taskbar_unique;
	TaskbarWindow* taskbar;
	Context& ctx;
	MouseState mouse_state {};
	uint32_t drag_x_off {};
	uint32_t drag_y_off {};
	Window* dragging {};
	bool key_states[SCANCODE_MAX] {};
};
