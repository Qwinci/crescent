#pragma once
#include "types.h"
#include "bus.h"

typedef struct {
	Bus bus;
} Emulator;

Emulator* emu_new();
bool emu_load_boot_rom(Emulator* self, const char* path);
bool emu_load_rom(Emulator* self, const char* path);
void emu_run(Emulator* self);
