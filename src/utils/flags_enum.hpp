#pragma once
#include "type_traits.hpp"

#define FLAGS_ENUM(name) \
constexpr name operator|(const name& lhs, const name& rhs) { \
	using underlying = kstd::underlying_type_t<name>; \
	return static_cast<name>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs)); \
} \
constexpr bool operator&(const name& lhs, const name& rhs) { \
	using underlying = kstd::underlying_type_t<name>; \
	return static_cast<underlying>(lhs) & static_cast<underlying>(rhs); \
} \
constexpr name operator~(const name& value) { \
	using underlying = kstd::underlying_type_t<name>; \
	return static_cast<name>(~static_cast<underlying>(value)); \
} \
constexpr name& operator|=(name& lhs, const name& rhs) { \
	lhs = lhs | rhs; \
	return lhs; \
} \
constexpr name& operator&=(name& lhs, const name& rhs) { \
	using underlying = kstd::underlying_type_t<name>; \
	lhs = static_cast<name>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs)); \
	return lhs; \
} \
namespace __ ## name {}  \
using namespace __ ## name
