#include "bus.h"
#include <stdio.h>

void bus_cycle(Bus* self) {
	u8 old_div = self->timer.div >> 8;
	cpu_cycle(&self->cpu);
	// PPU uses T cycles
	for (u8 i = 0; i < 4; ++i) {
		ppu_clock(&self->ppu);
		apu_clock_channels(&self->apu);
	}
	if (old_div & 1 << 4 && !((self->timer.div >> 8) & 1 << 4)) {
		apu_clock(&self->apu);
	}
}

void bus_write(Bus* self, u16 addr, u8 value) {
	if (addr <= 0x7FFF) { // NOLINT(bugprone-branch-clone)
		self->cart.mapper->write(self->cart.mapper, addr, value);
	}
	else if (addr <= 0x9FFF) {
		self->ppu.vram[addr - 0x8000] = value;
	}
	else if (addr <= 0xBFFF) {
		self->cart.mapper->write(self->cart.mapper, addr, value);
	}
	else if (addr <= 0xDFFF) {
		self->wram[addr - 0xC000] = value;
	}
	else if (addr <= 0xFDFF) {
		self->wram[addr - 0xE000] = value;
	}
	else if (addr <= 0xFE9F) {
		self->ppu.oam[addr - 0xFE00] = value;
	}
	else if (addr <= 0xFF7F) {
		if (addr == 0xFF01) {
			self->serial_byte = value;
		}
		else if (addr == 0xFF02) {
			if (value == 0x81) {
				fprintf(stderr, "%c", self->serial_byte);
			}
		}
		else if (addr >= 0xFF04 && addr <= 0xFF07) {
			timer_write(&self->timer, addr, value);
		}
		else if (addr == 0xFF0F) {
			self->cpu.if_flag = value;
		}
		else if (addr >= 0xFF10 && addr <= 0xFF3F) {
			apu_write(&self->apu, addr, value);
		}
		else if (addr == 0xFF40) {
			self->ppu.lcdc = value;
		}
		else if (addr == 0xFF41) {
			self->ppu.stat &= 0b111;
			self->ppu.stat |= value & ~0b111;
		}
		else if (addr == 0xFF42) {
			self->ppu.scy = value;
		}
		else if (addr == 0xFF43) {
			self->ppu.scx = value;
		}
		else if (addr == 0xFF45) {
			self->ppu.lyc = value;
		}
		else if (addr == 0xFF46) {
			self->last_dma = value;
			u16 dma_src = (u16) value << 8;
			for (u8 i = 0; i < 160; ++i) {
				self->ppu.oam[i] = bus_read(self, dma_src + i);
			}
		}
		else if (addr == 0xFF47) {
			self->ppu.bg_palette = value;
		}
		else if (addr == 0xFF48) {
			self->ppu.ob_palette0 = value;
		}
		else if (addr == 0xFF49) {
			self->ppu.ob_palette1 = value;
		}
		else if (addr == 0xFF4A) {
			self->ppu.wy = value;
		}
		else if (addr == 0xFF4B) {
			self->ppu.wx = value;
		}
		else if (addr == 0xFF50 && value) {
			self->bootrom_mapped = false;
		}
		else {
			// todo
			//fprintf(stderr, "unimplemented io write 0x%X\n", addr);
		}
	}
	else if (addr <= 0xFFFE) {
		self->hram[addr - 0xFF80] = value;
	}
	else {
		self->cpu.ie = value;
	}
}

u8 bus_read(Bus* self, u16 addr) {
	if (addr <= 0x7FFF) { // NOLINT(bugprone-branch-clone)
		if (addr < 0x100 && self->bootrom_mapped) {
			return self->boot_rom[addr];
		}
		else {
			return self->cart.mapper->read(self->cart.mapper, addr);
		}
	}
	else if (addr <= 0x9FFF) {
		return self->ppu.vram[addr - 0x8000];
	}
	else if (addr <= 0xBFFF) {
		return self->cart.mapper->read(self->cart.mapper, addr);
	}
	else if (addr <= 0xDFFF) {
		return self->wram[addr - 0xC000];
	}
	else if (addr <= 0xFDFF) {
		return self->wram[addr - 0xE000];
	}
	else if (addr <= 0xFE9F) {
		return self->ppu.oam[addr - 0xFE00];
	}
	else if (addr <= 0xFF7F) {
		if (addr == 0xFF00) {
			return self->joyp;
		}
		else if (addr >= 0xFF04 && addr <= 0xFF07) {
			return timer_read(&self->timer, addr);
		}
		else if (addr == 0xFF0F) {
			return self->cpu.if_flag;
		}
		else if (addr == 0xFF10) {
			return self->apu.nr10 | 0x80;
		}
		else if (addr == 0xFF11) {
			return self->apu.nr11 | 0x3F;
		}
		else if (addr == 0xFF12) {
			return self->apu.nr12;
		}
		else if (addr == 0xFF13) { // NOLINT(bugprone-branch-clone)
			return 0xFF;
		}
		else if (addr == 0xFF14) {
			return self->apu.nr14 | 0xBF;
		}
		else if (addr == 0xFF15) {
			return 0xFF;
		}
		else if (addr == 0xFF16) {
			return self->apu.nr21 | 0x3F;
		}
		else if (addr == 0xFF17) {
			return self->apu.nr22;
		}
		else if (addr == 0xFF18) {
			return 0xFF;
		}
		else if (addr == 0xFF19) {
			return self->apu.nr24 | 0xBF;
		}
		else if (addr == 0xFF1A) {
			return self->apu.nr30 | 0x7F;
		}
		else if (addr == 0xFF1B) {
			return 0xFF;
		}
		else if (addr == 0xFF1C) {
			return self->apu.nr32 | 0x9F;
		}
		else if (addr == 0xFF1D) {
			return 0xFF;
		}
		else if (addr == 0xFF1E) {
			return self->apu.nr34 | 0xBF;
		}
		else if (addr == 0xFF1F) {
			return 0xFF;
		}
		else if (addr == 0xFF20) {
			return 0xFF;
		}
		else if (addr == 0xFF21) {
			return self->apu.nr42;
		}
		else if (addr == 0xFF22) {
			return self->apu.nr43;
		}
		else if (addr == 0xFF23) {
			return self->apu.nr44 | 0xBF;
		}
		else if (addr == 0xFF24) {
			return self->apu.nr50;
		}
		else if (addr == 0xFF25) {
			return self->apu.nr51;
		}
		else if (addr == 0xFF26) {
			return self->apu.nr52 | 0x70;
		}
		else if (addr >= 0xFF27 && addr <= 0xFF2F) {
			return 0xFF;
		}
		else if (addr >= 0xFF30 && addr <= 0xFF3F) {
			return self->apu.wave_pattern[addr - 0xFF30];
		}
		else if (addr == 0xFF40) {
			return self->ppu.lcdc;
		}
		else if (addr == 0xFF41) {
			return self->ppu.stat;
		}
		else if (addr == 0xFF42) {
			return self->ppu.scy;
		}
		else if (addr == 0xFF43) {
			return self->ppu.scx;
		}
		else if (addr == 0xFF44) {
			return self->ppu.ly;
		}
		else if (addr == 0xFF45) {
			return self->ppu.lyc;
		}
		else if (addr == 0xFF46) {
			return self->last_dma;
		}
		else if (addr == 0xFF47) {
			return self->ppu.bg_palette;
		}
		else if (addr == 0xFF48) {
			return self->ppu.ob_palette0;
		}
		else if (addr == 0xFF49) {
			return self->ppu.ob_palette1;
		}
		else if (addr == 0xFF4A) {
			return self->ppu.wy;
		}
		else if (addr == 0xFF4B) {
			return self->ppu.wx;
		}
		// todo
		//fprintf(stderr, "unimplemented io read 0x%X\n", addr);
		return 0xFF;
	}
	else if (addr <= 0xFFFE) {
		return self->hram[addr - 0xFF80];
	}
	else {
		return self->cpu.ie;
	}
}
