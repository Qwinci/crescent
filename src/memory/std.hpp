#pragma once
#include "types.hpp"

extern "C" void* memset(void* dest, int ch, usize size);
extern "C" void* memcpy(void* dest, const void* src, usize size);