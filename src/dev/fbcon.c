#include "fbcon.h"
#include "con.h"
#include "fb.h"
#include "sched/sched.h"

typedef struct {
	u32 magic;
	u32 version;
	u32 header_size;
	u32 flags;
	u32 num_glyph;
	u32 bytes_per_glyph;
	u32 height;
	u32 width;
} PsfFont;

typedef struct {
	Con common;
	Framebuffer* fb;
	const PsfFont* font;
} FbCon;

static int fbcon_write(Con* self, char c);
static int fbcon_write_at(Con* self, usize x, usize y, char c);

static FbCon fbcon = {
	.common = {
		.write = fbcon_write,
		.write_at = fbcon_write_at,
		.column = 0,
		.line = 0,
		.fg = 0x00FF00,
		.bg = 0
	}
};

static int fbcon_write(Con* self, char c) {
	FbCon* fb = container_of(self, FbCon, common);
	if ((self->column + 1) * fb->font->width >= fb->fb->width) {
		self->column = 0;
		self->line += 1;
	}
	if (self->line * fb->font->height >= fb->fb->height) {
		/*
		fb_clear(fb->fb, 0);
		self->line = 0;
		self->column = 0;*/
		return 0;
		// todo
	}

	u32 bytes_per_line = (fb->font->width + 7) / 8;

	usize offset = sizeof(PsfFont) + (usize) c * fb->font->bytes_per_glyph;
	const u8* font_c = offset(fb->font, const u8*, offset);
	usize init_x = self->column * fb->font->width;
	usize init_y = self->line * fb->font->height;
	for (usize y = 0; y < fb->font->height; ++y) {
		for (usize x = 0; x < fb->font->width; ++x) {
			u32 shift = fb->font->width - 1 - x;
			u32 color = font_c[shift / 8] & (1 << (shift % 8)) ? self->fg : self->bg;
			fb_set_pixel(fb->fb, init_x + x, init_y + y, color);
		}
		font_c += bytes_per_line;
	}
	self->column += 1;

	return 1;
}

static int fbcon_write_at(Con* self, usize x, usize y, char c) {
	FbCon* fb = container_of(self, FbCon, common);
	if ((x + 1) * fb->font->width >= fb->fb->width) {
		x = 0;
		y += 1;
	}
	if (y * fb->font->height >= fb->fb->height) {
		return 0;
	}

	u32 bytes_per_line = (fb->font->width + 7) / 8;

	usize offset = sizeof(PsfFont) + (usize) c * fb->font->bytes_per_glyph;
	const u8* font_c = offset(fb->font, const u8*, offset);
	usize init_x = x * fb->font->width;
	usize init_y = y * fb->font->height;
	for (usize py = 0; y < fb->font->height; ++py) {
		for (usize px = 0; x < fb->font->width; ++px) {
			u32 shift = fb->font->width - 1 - px;
			u32 color = font_c[shift / 8] & (1 << (shift % 8)) ? self->fg : self->bg;
			fb_set_pixel(fb->fb, init_x + px, init_y + py, color);
		}
		font_c += bytes_per_line;
	}

	return 1;
}

void fbcon_init(Framebuffer* fb, const void* font) {
	fbcon.fb = fb;
	fbcon.font = (const PsfFont*) font;
	kernel_con = &fbcon.common;
}
