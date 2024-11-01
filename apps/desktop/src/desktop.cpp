#include "desktop.hpp"
#include <cassert>

Desktop::Desktop(ui::Context& ctx) : gui {ctx, ctx.width, ctx.height - TaskbarWindow::HEIGHT} {
	assert(ctx.height > TaskbarWindow::HEIGHT);

	auto taskbar_unique = std::make_unique<TaskbarWindow>(ctx.width);
	taskbar_unique->set_pos(0, ctx.height - TaskbarWindow::HEIGHT);
	taskbar = static_cast<TaskbarWindow*>(taskbar_unique.get());
	gui.root_windows.push_back(std::move(taskbar_unique));

	ctx.dirty_rects.push_back({
		.x = 0,
		.y = ctx.height - TaskbarWindow::HEIGHT,
		.width = ctx.width,
		.height = TaskbarWindow::HEIGHT
	});
}

void Desktop::draw() {
	gui.draw();
}

void Desktop::handle_mouse(ui::MouseState new_state) {
	gui.handle_mouse(new_state);
}

void Desktop::handle_keyboard(ui::KeyState new_state) {
	gui.handle_keyboard(new_state);
}
