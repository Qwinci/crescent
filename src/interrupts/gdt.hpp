#pragma once
#include "console.hpp"
#include "types.hpp"
#include "utils.hpp"

struct Tss;

void load_gdt(Tss* tss);
