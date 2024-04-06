#pragma once
#include "stdio.hpp"

#define assert(...) ((__VA_ARGS__) ? (void) 0 : panic(__FILE_NAME__, ":", __LINE__, ": (", __func__, ") assertion '", #__VA_ARGS__, "' failed\n"))
