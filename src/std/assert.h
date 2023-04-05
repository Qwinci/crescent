#pragma once
#include "stdio.h"

#define assert(expr) ((expr) ? (void) 0 : panic("%s:%u: (%s) assertion '%s' failed", __FILE_NAME__, __LINE__, #expr))

#define static_assert _Static_assert