#pragma once
#include "window.hpp"

class ButtonWindow : public Window {
public:
	ButtonWindow();

	void draw(Context& ctx) override;
	bool handle_mouse(Context& ctx, const MouseState& old_state, const MouseState& new_state) override;
	void on_mouse_leave(Context& ctx, const MouseState& new_state) override;

	bool handle_keyboard(Context& ctx, const KeyState& old_state, const KeyState& new_state) override {
		return false;
	}

	using Callback = void (*)(void* arg);
	Callback callback {};
	void* arg {};
	uint32_t active_color {0xFF0000};
	uint32_t inactive_color {0x00FFFF};

private:
	bool prev_mouse_state {};
};
