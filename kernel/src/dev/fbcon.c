#include "fbcon.h"
#include "fb.h"
#include "log.h"

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
	LogSink common;
	FbDev* fb;
	const PsfFont* font;
	u32 column;
	u32 line;
	u32 fg;
	u32 bg;
} FbLogSink;

static int fblog_write(LogSink* log_self, Str str);

static FbLogSink FBLOG = {
	.common = {
		.write = fblog_write
	},
	.fg = 0x00FF00
};

static int fblog_write(LogSink* log_self, Str str) {
	FbLogSink* self = container_of(log_self, FbLogSink, common);

	u32 bytes_per_line = (self->font->width + 7) / 8;

	for (usize i = 0; i < str.len; ++i) {
		if ((self->column + 1) * self->font->width >= self->fb->info.width) {
			self->column = 0;
			self->line += 1;
		}
		if (self->line * self->font->height >= self->fb->info.height) {
			fb_clear(self->fb, 0);
			self->line = 0;
			self->column = 0;
			// todo proper scrolling
		}

		char c = str.data[i];
		if (c == '\n') {
			self->column = 0;
			self->line += 1;
		}
		else if (c == '\t') {
			self->column += 4 - (self->column % 4);
		}
		else if (c == '\r') {
			self->column = 0;
		}
		else if (c == -1) {
			memcpy(&self->fg, &str.data[i + 1], 4);
			i += 4;
		}
		else if (c == -2) {
			memcpy(&self->bg, &str.data[i + 1], 4);
			i += 4;
		}
		else {
			u32 offset = sizeof(PsfFont) + (u32) c * self->font->bytes_per_glyph;
			const u8* font_c = offset(self->font, const u8*, offset);
			u32 init_x = self->column * self->font->width;
			u32 init_y = self->line * self->font->height;
			for (u32 y = 0; y < self->font->height; ++y) {
				for (u32 x = 0; x < self->font->width; ++x) {
					u32 shift = self->font->width - 1 - x;
					u32 color = font_c[shift / 8] & (1 << (shift % 8)) ? self->fg : self->bg;
					fb_set_pixel(self->fb, init_x + x, init_y + y, color);
				}
				font_c += bytes_per_line;
			}
			self->column += 1;
		}
	}

	return 0;
}

void fbcon_init(FbDev* fb, const void* font) {
	FBLOG.fb = fb;
	FBLOG.font = (const PsfFont*) font;
	log_register_sink(&FBLOG.common, false);
}
