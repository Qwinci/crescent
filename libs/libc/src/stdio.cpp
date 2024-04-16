#include "stdio.h"
#include "sys.hpp"
#include "string.h"

int puts(const char* str) {
	auto len = strlen(str);
	sys_syslog(str, len);
	return static_cast<int>(len);
}
