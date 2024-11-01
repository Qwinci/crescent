#pragma once
#include "primitive.hpp"

namespace ui {
	struct MouseState {
		Point pos;
		bool left_pressed;
		bool right_pressed;
		bool middle_pressed;
	};
}
