#pragma once
#include "types.hpp"

void random_add_entropy(const u64* data, usize words, u32 credible_bits);
void random_generate(void* data, usize size);
