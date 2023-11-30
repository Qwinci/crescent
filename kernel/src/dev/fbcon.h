#pragma once

typedef struct FbDev FbDev;

void fbcon_init(FbDev* fb, const void* font);
void default_fblog_deinit();
