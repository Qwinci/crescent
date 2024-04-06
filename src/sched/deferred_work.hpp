#pragma once
#include "double_list.hpp"
#include "functional.hpp"

struct DeferredIrqWork {
	DoubleListHook hook {};
	kstd::small_function<void()> fn;
};
