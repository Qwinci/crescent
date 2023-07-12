#include "crescent/gal.h"

void GalEnumerateFrameBuffers(GalFrameBuffer* fbs, int* count);

GalFrameBuffer* GalGetPrimaryFrameBuffer();
void GalCreateBackBuffer(const GalFrameBuffer* target, GalBackBuffer* back_buffer);
void GalDeleteBackBuffer(GalBackBuffer* back_buffer);
void GalSwapBuffers(const GalFrameBuffer* target, GalBackBuffer* back_buffer);