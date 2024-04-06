#pragma once
#include "types.hpp"

u32 x86_alloc_irq(u32 count, bool shared);
void x86_dealloc_irq(u32 irq, u32 count, bool shared);
