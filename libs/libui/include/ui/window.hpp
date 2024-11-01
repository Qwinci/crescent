#pragma once
#include <vector>
#include <memory>
#include <string_view>
#include "context.hpp"
#include "mouse.hpp"
#include "keyboard.hpp"

namespace ui {
	struct Window {
		explicit Window(bool no_decorations);
		virtual ~Window() = default;

		virtual void draw(Context& ctx);
		virtual void draw_decorations(Context& ctx);

		virtual bool handle_mouse(Context& ctx, const MouseState& old_state, const MouseState& new_state) {
			return false;
		}

		virtual void on_mouse_enter(Context& ctx, const MouseState& new_state) {}
		virtual void on_mouse_leave(Context& ctx, const MouseState& new_state) {}

		virtual bool handle_keyboard(Context& ctx, const KeyState& old_state, const KeyState& new_state) {
			return false;
		}

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

		[[nodiscard]] constexpr Point get_abs_pos_offset() const {
			Point pos {};
			auto* window = this;
			while (window->parent) {
				pos.x += window->parent->rect.x;
				pos.y += window->parent->rect.y;
				if (!window->parent->no_decorations) {
					pos.x += window->parent->border.left;
					pos.y += window->parent->titlebar->rect.height;
				}
				window = window->parent;
			}

			if (parent && parent->is_titlebar) {
				pos.x -= parent->parent->border.left;
				pos.y -= parent->rect.height;
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
		std::unique_ptr<Window> titlebar {};
		uint32_t* fb {};
		void (*on_close)(Window* window, void* arg) {};
		void* on_close_arg {};
		Rect rect {};
		Border border {};
		uint32_t bg_color {};
		uint32_t border_color {};
		bool no_decorations {};
		bool internal {};
		bool is_titlebar {};
	};
}
