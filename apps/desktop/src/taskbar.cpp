#include "taskbar.hpp"
#include "button.hpp"
#include "text.hpp"
#include "sys.hpp"

TaskbarWindow::TaskbarWindow(uint32_t width) : Window {true} {
	rect.width = width;
	rect.height = HEIGHT;
	bg_color = BG_COLOR;
	internal = true;

	CrescentDateTime date {};
	if (sys_get_date_time(date) == 0) {
		auto time_text_unique = std::make_unique<TextWindow>();
		auto date_text_unique = std::make_unique<TextWindow>();

		// todo don't hardcode font width here
		constexpr uint32_t date_text_width = 10 * 8;

		time_text_unique->set_size(date_text_width, rect.height / 2);
		time_text_unique->set_pos(rect.width - date_text_width, rect.y);

		date_text_unique->set_size(date_text_width, rect.height / 2);
		date_text_unique->set_pos(rect.width - date_text_width, rect.y + rect.height / 2);

		time_text = time_text_unique.get();
		date_text = date_text_unique.get();

		add_child(std::move(time_text_unique));
		add_child(std::move(date_text_unique));
	}
}

void TaskbarWindow::add_icon(uint32_t color) {
	auto window = std::make_unique<ButtonWindow>();
	window->bg_color = color;
	window->inactive_color = color;

	window->rect.width = rect.height;
	window->rect.height = rect.height;

	window->rect.x = last_x + last_width;
	last_x = window->rect.x;
	last_width = window->rect.width;

	add_child(std::move(window));
}

void TaskbarWindow::add_entry(std::unique_ptr<Window> entry) {
	entry->rect.width = rect.height;
	entry->rect.height = rect.height;

	entry->rect.x = last_x + last_width;
	last_x = entry->rect.x;
	last_width = entry->rect.width;

	add_child(std::move(entry));
}

void TaskbarWindow::update_time(Context& ctx) {
	CrescentDateTime date {};
	if (sys_get_date_time(date) == 0 && time_text) {
		char buffer[64];
		char* ptr = buffer + 64;
		auto print_number = [&](uintptr_t value, uint32_t pad) {
			auto start = ptr;
			do {
				*--ptr = "0123456789"[value % 10];
				value /= 10;
			} while (value);

			for (size_t i = (start - ptr); i < pad; ++i) {
				*--ptr = '0';
			}
		};

		print_number(date.minute, 2);
		*--ptr = '.';
		print_number(date.hour, 2);
		*--ptr = ' ';
		*--ptr = ' ';

		time_text->text = std::string_view {ptr, static_cast<size_t>((buffer + 64) - ptr)};

		ptr = buffer + 64;

		print_number(date.year, 4);
		*--ptr = '.';
		print_number(date.month, 2);
		*--ptr = '.';
		print_number(date.day, 2);

		date_text->text = std::string_view {ptr, static_cast<size_t>((buffer + 64) - ptr)};

		ctx.dirty_rects.push_back(get_abs_rect());
	}
}
