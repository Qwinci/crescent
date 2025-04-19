#include "no_mbc.h"
#include <stdlib.h>

void no_mbc_write(Mapper* self, u16 addr, u8 value);
u8 no_mbc_read(Mapper* self, u16 addr);

Mapper* no_mbc_new(Cart* self) {
	Mapper* mapper = malloc(sizeof(Mapper));
	if (!mapper) {
		return NULL;
	}
	mapper->cart = self;
	mapper->read = no_mbc_read;
	mapper->write = no_mbc_write;
	return mapper;
}

void no_mbc_write(Mapper* self, u16 addr, u8 value) {
	if (addr < 0xA000) {
		return;
	}
	// call invariant: addr >= 0xA000 && addr <= 0xBFFF && ram_size
	self->cart->ram[addr - 0xA000] = value;
}

u8 no_mbc_read(Mapper* self, u16 addr) {
	// call invariant: addr <= 0x7FFF || (addr >= 0xA000 && addr <= 0xBFFF && ram_size)
	if (addr <= 0x7FFF) {
		return self->cart->data[addr];
	}
	else {
		return self->cart->ram[addr - 0xA000];
	}
}
