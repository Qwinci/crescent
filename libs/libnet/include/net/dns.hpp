#pragma once
#include "ip.hpp"
#include <string_view>
#include <optional>

namespace dns {
	std::optional<Ipv4> resolve4(std::string_view domain);
}
