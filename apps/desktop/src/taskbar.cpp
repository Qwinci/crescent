#include "taskbar.hpp"
#include "button.hpp"

TaskbarWindow::TaskbarWindow(uint32_t width) : Window {true} {
	rect.width = width;
	rect.height = HEIGHT;
	bg_color = BG_COLOR;
	internal = true;
}

void TaskbarWindow::add_icon(uint32_t color) {
	auto window = std::make_unique<ButtonWindow>();
	window->bg_color = color;
	window->inactive_color = color;

	window->rect.width = rect.height;
	window->rect.height = rect.height;

	if (!children.empty()) {
		auto& last = children.back();
		window->rect.x = last->rect.x + last->rect.width;
	}

	add_child(std::move(window));
}

void TaskbarWindow::add_entry(std::unique_ptr<Window> entry) {
	entry->rect.width = rect.height;
	entry->rect.height = rect.height;

	if (!children.empty()) {
		auto& last = children.back();
		entry->rect.x = last->rect.x + last->rect.width;
	}
	else {
		entry->rect.x = 0;
	}

	add_child(std::move(entry));
}
