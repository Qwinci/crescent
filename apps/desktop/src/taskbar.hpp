#pragma once
#include <ui/window.hpp>

namespace ui {
	struct TextWindow;
}

struct TaskbarWindow : public ui::Window {
	static constexpr uint32_t HEIGHT = 30;
	static constexpr uint32_t BG_COLOR = 0xCFCFCF;

	explicit TaskbarWindow(uint32_t width);

	void add_icon(uint32_t color);
	void add_entry(std::unique_ptr<Window> entry);

	void update_time(ui::Context& ctx);

	bool handle_mouse(ui::Context& ctx, const ui::MouseState& old_state, const ui::MouseState& new_state) override {
		return false;
	}

	bool handle_keyboard(ui::Context& ctx, const ui::KeyState& old_state, const ui::KeyState& new_state) override {
		return false;
	}

	ui::TextWindow* time_text {};
	ui::TextWindow* date_text {};
	uint32_t last_x {};
	uint32_t last_width {};
};
