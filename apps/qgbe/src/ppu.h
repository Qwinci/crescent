#pragma once
#include "types.h"

typedef struct {
	u8 x;
	u8 y;
	u8 tile_num;
	u8 flags;
} OamEntry;

typedef enum {
	PPU_MODE_OAM_SCAN = 2,
	PPU_MODE_DRAW = 3,
	PPU_MODE_H_BLANK = 0,
	PPU_MODE_V_BLANK = 1
} PpuMode;

typedef enum {
	FETCH_STATE_TILE_NUM,
	FETCH_STATE_TILE_LOW,
	FETCH_STATE_TILE_HIGH,
	FETCH_STATE_PUSH
} FetchState;

typedef struct Ppu {
	struct Bus* bus;
	u32* texture;
	u32 cycle;
	OamEntry sprites[10];
	u8 vram[1024 * 8];
	u8 oam[160];
	u8 lcdc;
	u8 ly;
	u8 lyc;
	u8 stat;
	u8 scy;
	u8 scx;
	u8 wy;
	u8 wx;
	u8 bg_palette;
	u8 ob_palette0;
	u8 ob_palette1;
	bool frame_ready;
	u8 bg_fetcher_x;
	u8 lcd_x;
	u8 sprite_count;
	u8 sprite_ptr;

	u32 bg_fifo;
	u8 bg_fifo_size;
	u8 bg_fifo_discard;
	FetchState bg_fetch_state;

	u32 sprite_fifo;
	u8 sprite_fifo_size;
	FetchState sprite_fetch_state;
	OamEntry* sprite;
	u8 sprite_tile_low;
	u8 sprite_tile_high;

	u16 bg_tile_data_area;
	u8 bg_tile_num;
	u8 bg_tile_low;
	u8 bg_tile_high;
	u8 window_tile_x;
	u8 window_y;
	bool wx_triggered;
	bool wy_triggered;

	bool fetching_sprite;
} Ppu;

void ppu_reset(Ppu* self);
void ppu_clock(Ppu* self);
void ppu_generate_tile_map(Ppu* self, u32 width, u32 height, u32* data);
void ppu_generate_sprite_map(Ppu* self, u32 width, u32 height, u32* data);
