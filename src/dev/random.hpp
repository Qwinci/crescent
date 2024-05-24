#pragma once
#include "types.hpp"

void random_add_entropy(const u64* data, usize words);
void random_generate(void* data, usize size);
