#include "file.h"

static usize dummy_read(File* self, char* buf, usize size) {
	return 0;
}

static usize dummy_write(File* self, const char* buf, usize size) {
	return size;
}

static usize dummy_seek(File* self, usize off, SeekPos pos) {
	return 0;
}

File DUMMY_FILE = {
	.read = dummy_read,
	.write = dummy_write,
	.seek = dummy_seek
};