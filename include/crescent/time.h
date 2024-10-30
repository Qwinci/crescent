#ifndef CRESCENT_TIME_H
#define CRESCENT_TIME_H

#include <stdint.h>

typedef struct {
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
} CrescentDateTime;

#endif
