#pragma once
#include "window.hpp"

class ButtonWindow : public Window {
public:
	ButtonWindow();

	void draw(Context& ctx) override;
	bool handle_mouse(Context& ctx, const MouseState &new_state) override;
	void on_mouse_leave(Context &ctx, const MouseState &new_state) override;

	using Callback = void (*)(void* arg);
	Callback callback {};
	void* arg {};

private:
	bool prev_mouse_state {};
};
