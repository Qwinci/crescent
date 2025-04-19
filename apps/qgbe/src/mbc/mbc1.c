#include "mbc1.h"
#include <stdlib.h>

typedef struct {
	Mapper common;
	bool enable_ram;
	u8 rom_bank;
	u8 ram_bank;
	u8 banking_mode;
	bool extended_rom;
} Mbc1;

void mbc1_write(Mapper* self, u16 addr, u8 value);
u8 mbc1_read(Mapper* self, u16 addr);

Mapper* mbc1_new(Cart* self) {
	Mbc1* mapper = malloc(sizeof(Mbc1));
	if (!mapper) {
		return NULL;
	}
	mapper->common.cart = self;
	mapper->common.read = mbc1_read;
	mapper->common.write = mbc1_write;
	mapper->enable_ram = false;
	mapper->rom_bank = 1;
	mapper->ram_bank = 0;
	mapper->banking_mode = 0;
	mapper->extended_rom = self->rom_size >= 1024 * 1024;
	return &mapper->common;
}

void mbc1_write(Mapper* mapper_self, u16 addr, u8 value) {
	Mbc1* self = container_of(mapper_self, Mbc1, common);

	if (addr <= 0x1FFF) {
		self->enable_ram = (value & 0xF) == 0xA;
	}
	else if (addr <= 0x3FFF) {
		value &= 0b11111;
		u8 rom_bank_num;
		if (value == 0) {
			rom_bank_num = 1;
		}
		else {
			rom_bank_num = value & (self->common.cart->num_rom_banks - 1);
		}
		self->rom_bank = rom_bank_num;
	}
	else if (addr <= 0x5FFF) {
		if (self->common.cart->ram_size == 32 * 1024) {
			value &= 0b11;
			self->ram_bank = value;
		}
		else if (self->common.cart->rom_size >= 1024 * 1024) {
			u8 mask = self->common.cart->num_rom_banks >> 5;
			value &= mask - 1;
			self->ram_bank = value;
		}
	}
	else if (addr <= 0x7FFF) {
		if (mapper_self->cart->ram_size > 1024 * 8 ||
			mapper_self->cart->rom_size > 1024 * 512) {
			self->banking_mode = value;
		}
	}
	else if (addr >= 0xA000 && addr <= 0xBFFF && self->enable_ram) {
		Cart* cart = mapper_self->cart;
		if (self->banking_mode == 0 || self->extended_rom) {
			cart->ram[(addr - 0xA000) & 0x1FFF] = value;
		}
		else {
			cart->ram[((addr - 0xA000) & 0x1FFF) | self->ram_bank << 13] = value;
		}
	}
}

u8 mbc1_read(Mapper* mapper_self, u16 addr) {
	Mbc1* self = container_of(mapper_self, Mbc1, common);
	Cart* cart = mapper_self->cart;

	if (addr <= 0x3FFF) {
		u32 new_addr = addr;
		if (self->banking_mode == 0 || !self->extended_rom) {
			return cart->data[new_addr];
		}
		else {
			return cart->data[new_addr | (u32) self->ram_bank << 19];
		}
	}
	else if (addr <= 0x7FFF) {
		u32 new_addr = addr & 0x3FFF;
		new_addr |= (u32) self->rom_bank << 14;
		new_addr |= (u32) self->ram_bank << 19;
		new_addr &= cart->rom_size - 1;
		return cart->data[new_addr];
	}
	else if (addr >= 0xA000 && addr <= 0xBFFF && self->enable_ram) {
		if (self->banking_mode == 0 || self->extended_rom) {
			return cart->ram[(addr - 0xA000) & 0x1FFF];
		}
		else {
			return cart->ram[((addr - 0xA000) & 0x1FFF) | self->ram_bank << 13];
		}
	}

	return 0xFF;
}
