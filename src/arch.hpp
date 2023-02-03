#pragma once
#include "fb.hpp"

struct PsfFont;

void arch_init_mem();
void arch_init_smp(void (*fn)(u8 id, u32 acpi_id));
void* arch_get_rsdp();
const PsfFont* arch_get_font();
Framebuffer arch_get_framebuffer();
void* arch_get_module(const char* name);