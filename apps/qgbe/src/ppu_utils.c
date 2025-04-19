#include "ppu.h"

static u32 PALETTE_COLORS[] = {
	[0] = 0xFFFFFFFF,
	[1] = 0xD3D3D3FF,
	[2] = 0x5A5A5AFF,
	[3] = 0x000000FF
};

void ppu_generate_tile_map(Ppu* self, u32 width, u32 height, u32* data) {
	u16 tile_index = 0;

	u16 scale = 1;
	usize x_tiles;
	while (true) {
		scale += 1;
		x_tiles = width / (scale * 8);
		usize y_tiles = height / (scale * 8);
		if (y_tiles * x_tiles < 128 * 3) {
			scale -= 1;
			x_tiles = width / (scale * 8);
			break;
		}
	}

	for (u32 y = 0; y < height; ++y) {
		u32 tmp_tile_index = tile_index;
		u16 x_tiles_placed = 0;
		for (u32 x = 0; x < width; ++x) {
			u16 off = 16 * tmp_tile_index + (y / scale % 8) * 2;
			u8 tile_low = self->vram[off];
			u8 tile_high = self->vram[off + 1];

			tile_low <<= x / scale % 8;
			tile_high <<= x / scale % 8;
			u8 color_id = (tile_low >> 7) | (tile_high >> 7) << 1;
			u8 color_idx = self->bg_palette >> (2 * color_id) & 0b11;

			data[y * width + x] = PALETTE_COLORS[color_idx];

			if (x % (scale * 8U) == scale * 8U - 1) {
				x_tiles_placed += 1;
				tmp_tile_index += 1;
			}

			if (x_tiles_placed == x_tiles) {
				break;
			}
		}

		if (y % (scale * 8U) == scale * 8U - 1) {
			tile_index += x_tiles;
		}

		if (tile_index >= 128 * 3) {
			break;
		}
	}
}

void ppu_generate_sprite_map(Ppu* self, u32 width, u32 height, u32* data) {
	u8 sprite_height = self->lcdc & 1 << 2 ? 16 : 8;

	u16 scale = 1;
	usize x_sprites;
	usize y_sprites;
	while (true) {
		scale += 1;
		x_sprites = width / (scale * 8);
		y_sprites = height / (scale * sprite_height);
		if (y_sprites * x_sprites < 40) {
			scale -= 1;
			x_sprites = width / (scale * 8);
			// y_sprites = height / (scale * sprite_height);
			break;
		}
	}

	u16 oam_index = 0;
	for (u32 y = 0; y < height; ++y) {
		u16 tmp_oam_index = oam_index;
		u8 x_sprites_placed = 0;
		for (u32 x = 0; x < width; ++x) {
			u8 tile_index = self->oam[tmp_oam_index + 2];
			u8 flags = self->oam[tmp_oam_index + 3];

			u16 off = 16 * tile_index + (y / scale % 8) * 2;
			u8 tile_low = self->vram[off];
			u8 tile_high = self->vram[off + 1];

			tile_low <<= x / scale % 8;
			tile_high <<= x / scale % 8;
			u8 color_id = (tile_low >> 7) | (tile_high >> 7) << 1;

			u8 palette = flags & 1 << 4 ? self->ob_palette1 : self->ob_palette0;
			u8 color_idx = palette >> (2 * color_id) & 0b11;

			data[y * width + x] = PALETTE_COLORS[color_idx];

			if (x % (scale * 8U) == scale * 8U - 1) {
				x_sprites_placed += 1;
				tmp_oam_index += 4;
			}

			if (x_sprites_placed == x_sprites || tmp_oam_index >= 40 * 4) {
				break;
			}
		}

		if (y % (scale * sprite_height) == scale * sprite_height - 1U) {
			oam_index += x_sprites * 4;
		}

		if (oam_index >= 40 * 4) {
			break;
		}
	}
}