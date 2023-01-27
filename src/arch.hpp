#pragma once
#include "fb.hpp"

struct PsfFont;

void arch_init_mem();
void arch_init_smp(__attribute__((noreturn)) void (*fn)(u8 id));
void* arch_get_rsdp();
const PsfFont* arch_get_font();
Framebuffer arch_get_framebuffer();