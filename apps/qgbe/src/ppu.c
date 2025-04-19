#include "ppu.h"
#include "bus.h"
#include <assert.h>
#include <string.h>

#define LCD_WIDTH 160
#define LCD_HEIGHT 144

#define SCANLINE_CYCLES 456

static u32 PALETTE_COLORS[] = {
	[0] = 0xFFFFFF,
	[1] = 0xD3D3D3,
	[2] = 0x5A5A5A,
	[3] = 0x000000
};

#define STAT_IRQ_LYC (1 << 6)
#define STAT_IRQ_OAM_SCAN (1 << 5)
#define STAT_IRQ_V_BLANK (1 << 4)
#define STAT_IRQ_H_BLANK (1 << 3)
#define STAT_LYC (1 << 2)

static inline void ppu_set_mode(Ppu* self, PpuMode mode) {
	self->stat &= ~0b11;
	self->stat |= mode;
}

static inline PpuMode ppu_get_mode(Ppu* self) {
	return (PpuMode) (self->stat & 0b11);
}

static void ppu_change_mode(Ppu* self, PpuMode mode) {
	bool irq = false;
	switch (mode) {
		case PPU_MODE_OAM_SCAN:
			irq = self->stat & STAT_IRQ_OAM_SCAN;
			self->cycle = 0;
			self->wy_triggered = self->ly >= self->wy;
			self->sprite_count = 0;
			self->sprite = NULL;
			self->sprite_ptr = 0;
			break;
		case PPU_MODE_DRAW:
			self->bg_fifo_size = 0;
			self->bg_fetcher_x = 0;
			self->bg_fetch_state = FETCH_STATE_TILE_NUM;
			self->lcd_x = 0;
			if (!self->wy_triggered) {
				self->bg_fifo_discard = self->scx % 8;
			}
			break;
		case PPU_MODE_H_BLANK:
			irq = self->stat & STAT_IRQ_H_BLANK;
			if (self->wx_triggered) {
				self->window_tile_x = 0;
				self->window_y += 1;
			}
			break;
		case PPU_MODE_V_BLANK:
			assert(self->ly == LCD_HEIGHT);
			assert(self->cycle < SCANLINE_CYCLES);
			irq = self->stat & STAT_IRQ_V_BLANK;
			cpu_request_irq(&self->bus->cpu, IRQ_VBLANK);
			self->window_tile_x = 0;
			self->window_y = 0;
			break;
	}

	if (irq) {
		cpu_request_irq(&self->bus->cpu, IRQ_LCD_STAT);
	}
	ppu_set_mode(self, mode);
}

void ppu_reset(Ppu* self) {
	ppu_change_mode(self, PPU_MODE_OAM_SCAN);
}

static void ppu_oam_scan(Ppu* self) {
	if (self->cycle++ % 2 != 0) {
		return;
	}
	if (self->cycle == 79) {
		ppu_change_mode(self, PPU_MODE_DRAW);
		return;
	}
	u8 height = self->lcdc & 1 << 2 ? 16 : 8;

	u8* entry = &self->oam[self->cycle * 2];
	u8 y = entry[0];
	u8 x = entry[1];
	u8 tile_num = entry[2];
	u8 flags = entry[3];

	if (self->ly + 16 >= y && self->ly + 16 < y + height) {
		if (self->sprite_count < 10) {
			u8 index = self->sprite_count;
			for (u8 i = 0; i < self->sprite_count; ++i) {
				if (self->sprites[i].x > x) {
					memmove(&self->sprites[i + 1], &self->sprites[i], (self->sprite_count - i) * sizeof(OamEntry));
					index = i;
					break;
				}
			}
			self->sprite_count += 1;
			self->sprites[index] = (OamEntry) {
				.x = x,
				.y = y,
				.tile_num = tile_num,
				.flags = flags
			};
		}
	}
}

static void ppu_draw(Ppu* self) {
	assert(self->ly < LCD_HEIGHT);
	if (self->cycle++ % 2 != 0) {
		return;
	}

	if (self->bg_fetch_state == FETCH_STATE_TILE_NUM) {
		if (self->wy_triggered && self->bg_fetcher_x >= self->wx - 7) {
			self->wx_triggered = true;
		}
		else {
			self->wx_triggered = false;
		}

		if (!(self->lcdc & 1 << 5)) {
			self->wx_triggered = false;
		}
		u16 tile_map_area;
		if ((self->lcdc & 1 << 3 && !self->wx_triggered) || (self->lcdc & 1 << 6 && self->wx_triggered)) {
			tile_map_area = 0x1C00;
		}
		else {
			tile_map_area = 0x1800;
		}

		u16 x_off;
		u16 y_off;
		if (self->wx_triggered) {
			x_off = self->window_tile_x & 0x1F;
			y_off = 32 * ((self->window_y / 8) & 0xFF);
		}
		else {
			x_off = ((self->scx + self->bg_fetcher_x) / 8) & 0x1F;
			y_off = 32 * (((self->ly + self->scy) / 8) & 0xFF);
		}

		self->bg_tile_num = self->vram[tile_map_area + ((x_off + y_off) & 0x3FF)];

		self->bg_tile_data_area = self->lcdc & 1 << 4 ? 0 : 0x1000;

		self->bg_fetch_state = FETCH_STATE_TILE_LOW;
	}
	else if (self->bg_fetch_state == FETCH_STATE_TILE_LOW) {
		u8 fine_y;
		if (self->wx_triggered) {
			fine_y = self->window_y % 8;
		}
		else {
			fine_y = (self->ly + self->scy) % 8;
		}
		if (self->bg_tile_data_area == 0x1000) {
			self->bg_tile_low = self->vram[self->bg_tile_data_area + fine_y * 2 + 16 * (i8) self->bg_tile_num];
		}
		else {
			self->bg_tile_low = self->vram[self->bg_tile_data_area + fine_y * 2 + 16 * self->bg_tile_num];
		}

		self->bg_fetch_state = FETCH_STATE_TILE_HIGH;
	}
	else if (self->bg_fetch_state == FETCH_STATE_TILE_HIGH) {
		u8 fine_y;
		if (self->wx_triggered) {
			fine_y = self->window_y % 8;
		}
		else {
			fine_y = (self->ly + self->scy) % 8;
		}

		if (self->bg_tile_data_area == 0x1000) {
			self->bg_tile_high = self->vram[self->bg_tile_data_area + fine_y * 2 + 16 * (i8) self->bg_tile_num + 1];
		}
		else {
			self->bg_tile_high = self->vram[self->bg_tile_data_area + fine_y * 2 + 16 * self->bg_tile_num + 1];
		}

		self->bg_fetch_state = FETCH_STATE_PUSH;

		if (self->wx_triggered) {
			self->window_tile_x += 1;
		}
	}
	else if (self->bg_fetch_state == FETCH_STATE_PUSH) {
		if (self->bg_fifo_size == 0) {
			self->bg_fifo = 0;
			for (u8 i = 0; i < 8; ++i) {
				self->bg_fifo <<= 3;
				self->bg_fifo |= (self->bg_tile_low >> 7) << 2 | ((self->bg_tile_high >> 7) << 1);
				self->bg_tile_low <<= 1;
				self->bg_tile_high <<= 1;
			}
			self->bg_fifo_size = 8;
			self->bg_fetch_state = FETCH_STATE_TILE_NUM;
			self->bg_fetcher_x += 8;
		}
	}
}

static void ppu_h_blank(Ppu* self) {
	if (++self->cycle == SCANLINE_CYCLES) {
		self->ly += 1;
		if (self->ly == self->lyc) {
			self->stat |= STAT_LYC;
			if (self->stat & STAT_IRQ_LYC) {
				cpu_request_irq(&self->bus->cpu, IRQ_LCD_STAT);
			}
		}
		else {
			self->stat &= ~STAT_LYC;
		}
		self->cycle = 0;
		if (self->ly == 144) {
			self->frame_ready = true;
			ppu_change_mode(self, PPU_MODE_V_BLANK);
		}
		else {
			ppu_change_mode(self, PPU_MODE_OAM_SCAN);
		}
	}
}

static void ppu_v_blank(Ppu* self) {
	if (++self->cycle == SCANLINE_CYCLES) {
		self->cycle = 0;
		self->ly += 1;
		if (self->ly == self->lyc) {
			self->stat |= STAT_LYC;
			if (self->stat & STAT_IRQ_LYC) {
				cpu_request_irq(&self->bus->cpu, IRQ_LCD_STAT);
			}
		}
		else {
			self->stat &= ~STAT_LYC;
		}
		if (self->ly == 154) {
			self->ly = 0;
			ppu_change_mode(self, PPU_MODE_OAM_SCAN);
		}
	}
}

static void ppu_lcd_push(Ppu* self) {
	if (!self->fetching_sprite && self->bg_fifo_size) {
		if (self->bg_fifo_discard) {
			self->bg_fifo <<= 3;
			self->bg_fifo_size -= 1;
			self->bg_fifo_discard -= 1;
			return;
		}

		assert(self->ly < LCD_HEIGHT);
		assert(self->lcd_x < LCD_WIDTH);

		u8 color_id;
		if (self->lcdc & 1 << 0) {
			color_id = (self->bg_fifo >> 23 & 1) | (self->bg_fifo >> 22 & 1) << 1;
		}
		else {
			color_id = 0;
		}

		u8 color_index = self->bg_palette >> (2 * color_id) & 0b11;
		self->texture[self->ly * LCD_WIDTH + self->lcd_x] = PALETTE_COLORS[color_index];

		self->lcd_x += 1;

		self->bg_fifo <<= 3;
		self->bg_fifo_size -= 1;
	}
}

void ppu_clock(Ppu* self) {
	if (!(self->lcdc & 1 << 7)) {
		return;
	}

	switch (ppu_get_mode(self)) {
		case PPU_MODE_OAM_SCAN:
			ppu_oam_scan(self);
			break;
		case PPU_MODE_DRAW:
			ppu_draw(self);
			ppu_lcd_push(self);
			if (self->lcd_x == LCD_WIDTH) {
				ppu_change_mode(self, PPU_MODE_H_BLANK);
			}
			break;
		case PPU_MODE_H_BLANK:
			ppu_h_blank(self);
			break;
		case PPU_MODE_V_BLANK:
			ppu_v_blank(self);
			break;
	}
}
