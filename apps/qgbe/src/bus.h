#pragma once
#include "apu.h"
#include "cart.h"
#include "cpu.h"
#include "ppu.h"
#include "timer.h"
#include "types.h"

typedef struct Bus {
	Cpu cpu;
	Ppu ppu;
	Apu apu;
	Cart cart;
	Timer timer;
	u8 wram[1024 * 8];
	u8 hram[127];
	u8 boot_rom[0x100];
	bool bootrom_mapped;
	bool serial_enabled;
	u8 serial_out;
	u8 serial_byte;
	u8 serial_cycle;
	u8 joyp;
	u8 last_dma;
} Bus;

void bus_write(Bus* self, u16 addr, u8 value);
u8 bus_read(Bus* self, u16 addr);
void bus_cycle(Bus* self);
