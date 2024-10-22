#pragma once
#include "window.hpp"
#include "libtext/libtext.hpp"
#include <string>
#include <unordered_map>

struct TextWindow : public Window {
	TextWindow();

	void draw(Context& ctx) override;

	bool handle_mouse(Context&, const MouseState&, const MouseState&) override {
		return false;
	}

	void on_mouse_enter(Context&, const MouseState&) override {}

	void on_mouse_leave(Context&, const MouseState&) override {}

	bool handle_keyboard(Context&, const KeyState&, const KeyState&) override {
		return false;
	}

	uint32_t text_color = 0xFFFFFF;
	std::string text;

private:
	libtext::Context text_ctx;
	std::unordered_map<uint32_t, libtext::Bitmap> glyph_cache {};
};