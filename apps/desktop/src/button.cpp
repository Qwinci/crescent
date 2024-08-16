#include "button.hpp"

ButtonWindow::ButtonWindow() : Window {true} {
	bg_color = inactive_color;
	internal = true;
}

void ButtonWindow::draw(Context& ctx) {
	ctx.draw_filled_rect(rect, bg_color);
}

bool ButtonWindow::handle_mouse(Context& ctx, const MouseState&, const MouseState& new_state) {
	if (!prev_mouse_state && new_state.left_pressed) {
		bg_color = active_color;
		prev_mouse_state = true;
		ctx.dirty_rects.push_back(get_abs_rect());
	}
	else if (prev_mouse_state && !new_state.left_pressed) {
		bg_color = inactive_color;
		prev_mouse_state = false;
		if (callback) {
			callback(arg);
		}
		ctx.dirty_rects.push_back(get_abs_rect());
	}

	return true;
}

void ButtonWindow::on_mouse_leave(Context& ctx, const MouseState& new_state) {
	if (prev_mouse_state) {
		bg_color = inactive_color;
		prev_mouse_state = false;
		ctx.dirty_rects.push_back(get_abs_rect());
	}
}
