#pragma once
#include "windower/protocol.hpp"

namespace protocol = windower::protocol;

void send_event_to_window(Window* window, protocol::WindowEvent event);
