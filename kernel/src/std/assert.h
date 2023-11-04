#pragma once
#include "stdio.h"

#define assert(expr) ((expr) ? (void) 0 : panic("%s:%u: (%s) assertion '%s' failed\n", __FILE_NAME__, __LINE__, __func__, #expr))

#define static_assert _Static_assert