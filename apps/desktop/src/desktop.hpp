#pragma once
#include <ui/gui.hpp>
#include "taskbar.hpp"

struct Desktop {
	explicit Desktop(ui::Context& ctx);

	void draw();
	void handle_mouse(ui::MouseState new_state);
	void handle_keyboard(ui::KeyState new_state);

	void add_child(std::unique_ptr<ui::Window> window) {
		gui.root_windows[0]->add_child(std::move(window));
	}

	ui::Gui gui;

	TaskbarWindow* taskbar;
	ui::Window* start_menu {};
};
