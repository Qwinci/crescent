#pragma once

typedef struct SysFramebuffer SysFramebuffer;

void fbcon_init(SysFramebuffer* fb, const void* font);