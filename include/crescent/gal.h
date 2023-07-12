#pragma once
#include <stdint.h>

typedef enum {
	GAL_FORMAT_BGRA32
} GalFormat;

typedef struct {
	void* base;
	uint64_t width;
	uint64_t height;
	uint64_t pitch;
	uint64_t bpp;
	GalFormat format;
} GalFrameBuffer;

typedef struct {
	void* base;
	uint64_t size;
} GalBackBuffer;

void GalEnumerateFrameBuffers(GalFrameBuffer* fbs, int* count);
GalFrameBuffer* GalGetPrimaryFrameBuffer();
void GalCreateBackBuffer(const GalFrameBuffer* target, GalBackBuffer* back_buffer);
void GalDeleteBackBuffer(GalBackBuffer* back_buffer);
void GalSwapBuffers(const GalFrameBuffer* target, GalBackBuffer* back_buffer);
