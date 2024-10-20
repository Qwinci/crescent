#pragma once
#include <vector>
#include <memory>
#include <string_view>
#include "context.hpp"
#include "mouse.hpp"
#include "keyboard.hpp"

static constexpr uint32_t TITLEBAR_HEIGHT = 20;
static constexpr uint32_t BORDER_WIDTH = 4;
static constexpr uint32_t BORDER_COLOR = 0xFFFFFF;
static constexpr uint32_t TITLEBAR_ACTIVE_COLOR = 0xFFFFFF;

struct Window {
	explicit Window(bool no_decorations);
	virtual ~Window() = default;

	virtual void draw(Context& ctx);
	virtual void draw_decorations(Context& ctx);

	virtual bool handle_mouse(Context& ctx, const MouseState& old_state, const MouseState& new_state);
	virtual void on_mouse_enter(Context& ctx, const MouseState& new_state);
	virtual void on_mouse_leave(Context& ctx, const MouseState& new_state);

	virtual bool handle_keyboard(Context& ctx, const KeyState& old_state, const KeyState& new_state);

	void draw_generic(Context& ctx, std::vector<Rect> parent_clip_rects);

	void add_child(std::unique_ptr<Window> child);

	constexpr void set_pos(uint32_t x, uint32_t y) {
		rect.x = x;
		rect.y = y;
	}

	void set_size(uint32_t width, uint32_t height) {
		rect.width = width;
		rect.height = height;
		if (titlebar) {
			titlebar->update_titlebar(width);
		}
	}

	virtual void update_titlebar(uint32_t width) {}

	void set_title(std::string_view title);

	[[nodiscard]] constexpr Point get_abs_pos_offset() const {
		Point pos {};
		auto* window = this;
		while (window->parent) {
			pos.x += window->parent->rect.x;
			pos.y += window->parent->rect.y;
			if (!window->parent->no_decorations) {
				pos.y += TITLEBAR_HEIGHT;
				pos.x += BORDER_WIDTH;
			}
			window = window->parent;
		}

		if (parent && parent->is_titlebar) {
			pos.x -= BORDER_WIDTH;
			pos.y -= TITLEBAR_HEIGHT;
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

	Window* parent {};
	Window* active_child {};
	std::vector<std::unique_ptr<Window>> children;
	std::unique_ptr<Window> titlebar;
	uint32_t* fb {};
	Rect rect {};
	uint32_t bg_color {};
	bool no_decorations {};
	bool internal {};
	bool is_titlebar {};
};
