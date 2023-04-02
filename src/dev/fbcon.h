#pragma once

typedef struct Framebuffer Framebuffer;

void fbcon_init(Framebuffer* fb, const void* font);