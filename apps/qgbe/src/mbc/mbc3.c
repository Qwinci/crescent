#include "mbc3.h"
#include <stdlib.h>

typedef struct {
	Mapper common;
} Mbc3;

u8 mbc3_read(Mapper* mapper_self, u16 addr);
void mbc3_write(Mapper* mapper_self, u16 addr, u8 value);

Mapper* mbc3_new(Cart* self) {
	Mbc3* mapper = malloc(sizeof(Mbc3));
	if (!mapper) {
		return NULL;
	}
	mapper->common.cart = self;
	mapper->common.read = mbc3_read;
	mapper->common.write = mbc3_write;
	return &mapper->common;
}

u8 mbc3_read(Mapper* mapper_self, u16 addr) {
	return 0;
}

void mbc3_write(Mapper* mapper_self, u16 addr, u8 value) {

}
