#pragma once
#include "types.h"

typedef enum {
	SEEK_SET,
	SEEK_END
} SeekPos;

typedef struct File {
	/// Reads at max size bytes from the file and returns how many was actually read
	usize (*read)(struct File* self, char* buf, usize size);
	/// Writes at max size bytes from buf and returns how many was actually written
	usize (*write)(struct File* self, const char* buf, usize size);
	/// Seeks the file cursor to the specified position and returns the previous offset from the start
	usize (*seek)(struct File* self, usize off, SeekPos pos);
} File;

extern File DUMMY_FILE;