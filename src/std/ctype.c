#include "ctype.h"

int tolower(int ch) {
	return ch | 1 << 5;
}