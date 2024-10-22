#pragma once
#include "window.hpp"

struct TaskbarWindow : public Window {
	static constexpr uint32_t HEIGHT = 30;
	static constexpr uint32_t BG_COLOR = 0xCFCFCF;

	explicit TaskbarWindow(uint32_t width);

	void add_icon(uint32_t color);

	bool handle_mouse(Context& ctx, const MouseState& old_state, const MouseState& new_state) override {
		return false;
	}

	bool handle_keyboard(Context& ctx, const KeyState& old_state, const KeyState& new_state) override {
		return false;
	}
};
