#include "inttypes.h"
#include "ctype.h"

static char chars[] = "0123456789abcdefghijklmnopqrstuvwxyz";

uintmax_t strtoumax(const char* restrict str, const char** restrict end, int base) {
	bool negate = false;
	if (*str == '+') {
		++str;
	}
	else if (*str == '-') {
		negate = true;
		++str;
	}

	if (base == 0) {
		if (str[0] == '0' && tolower(str[1]) == 'x') {
			base = 16;
			str += 2;
		}
		else if (str[0] == '0') {
			base = 8;
			++str;
		}
		else {
			base = 10;
		}
	}

	uintmax_t value = 0;
	if (base <= 36) {
		for (; *str >= '0' && tolower(*str) <= chars[base - 1]; ++str) {
			value = base * value + (*str <= '9' ? (*str - '0') : (tolower(*str) - 'a' + 10));
		}
	}

	if (end) {
		*end = str;
	}
	if (negate) {
		value = -value;
	}
	return value;
}
