#pragma once
#include "crescent/input.h"
#include "types.h"

typedef CrescentScancode (*OneByteScancodeTranslatorFn)(u8 byte);

void ps2_kb_init(bool second);
void ps2_kb_set_translation(OneByteScancodeTranslatorFn fn);