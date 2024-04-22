#pragma once
#include <vector>
#include <memory>
#include "context.hpp"
#include "mouse.hpp"

static constexpr uint32_t TITLEBAR_HEIGHT = 20;
static constexpr uint32_t TITLEBAR_ACTIVE_COLOR = 0xFFFFFF;

struct Window {
	virtual ~Window() = default;

	virtual void draw(Context& ctx);
	virtual bool handle_mouse(Context& ctx, const MouseState& new_state);
	virtual void on_mouse_enter(Context& ctx, const MouseState& new_state);
	virtual void on_mouse_leave(Context& ctx, const MouseState& new_state);

	void draw_generic(Context& ctx);

	void add_child(std::unique_ptr<Window> child);

	constexpr void set_pos(uint32_t x, uint32_t y) {
		rect.x = x;
		rect.y = y;
	}

	constexpr void set_size(uint32_t width, uint32_t height) {
		rect.width = width;
		rect.height = height;
	}

	[[nodiscard]] constexpr Point get_abs_pos_offset() const {
		Point pos {};
		auto* window = this;
		while (window->parent) {
			pos.x += window->parent->rect.x;
			pos.y += window->parent->rect.y;
			window = window->parent;
		}
		return pos;
	}

	[[nodiscard]] constexpr Rect get_abs_rect() const {
		auto offset = get_abs_pos_offset();
		return {
			.x = rect.x + offset.x,
			.y = rect.y + offset.y,
			.width = rect.width,
			.height = rect.height
		};
	}

	Window* parent;
	std::vector<std::unique_ptr<Window>> children;
	uint32_t* fb {};
	Rect rect {};
	uint32_t bg_color {};
	bool no_decorations {};
};
