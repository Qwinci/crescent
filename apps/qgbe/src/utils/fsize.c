#include "fsize.h"

#if defined(__unix__) || defined(__unix) || defined(__linux__) || defined(__linux) || defined(__APPLE__)
#include <sys/stat.h>

size_t fsize(const char* path) {
	struct stat s = {};
	stat(path, &s);
	return s.st_size;
}
#else
#include <stdio.h>

size_t fsize(const char* path) {
	FILE* file = fopen(path, "rb");
	if (!file) {
		return 0;
	}
	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	fclose(file);
	return size;
}
#endif