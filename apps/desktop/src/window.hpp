#pragma once
#include <vector>
#include <memory>
#include <string_view>
#include <ui/window.hpp>

static constexpr uint32_t TITLEBAR_HEIGHT = 20;
static constexpr uint32_t BORDER_WIDTH = 4;
static constexpr uint32_t BORDER_COLOR = 0xFFFFFF;
static constexpr uint32_t TITLEBAR_ACTIVE_COLOR = 0xFFFFFF;

struct DesktopWindow : public ui::Window {
	explicit DesktopWindow(bool no_decorations);
	~DesktopWindow() override = default;

	bool handle_mouse(ui::Context& ctx, const ui::MouseState& old_state, const ui::MouseState& new_state) override;
	void on_mouse_enter(ui::Context& ctx, const ui::MouseState& new_state) override;
	void on_mouse_leave(ui::Context& ctx, const ui::MouseState& new_state) override;

	bool handle_keyboard(ui::Context& ctx, const ui::KeyState& old_state, const ui::KeyState& new_state) override;

	void update_titlebar(uint32_t width) override {}

	void set_title(std::string_view title);
};
