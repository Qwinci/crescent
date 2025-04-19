#pragma once
#include "types.h"

typedef struct {
	u8 jmp[4];
	u8 logo[48];
	u8 title[16];
	u16 new_licensee;
	u8 sgb;
	u8 type;
	u8 rom_size;
	u8 ram_size;
	u8 dest_code;
	u8 old_licensee;
	u8 mask_rom_version;
	u8 hdr_checksum;
	u16 global_checksum;
} CartHdr;
static_assert(sizeof(CartHdr) == 80);

typedef struct Cart Cart;

typedef struct Mapper {
	void (*write)(struct Mapper* self, u16 addr, u8 value);
	u8 (*read)(struct Mapper* self, u16 addr);
	Cart* cart;
} Mapper;

typedef struct Cart {
	Mapper* mapper;
	const CartHdr* hdr;
	u8* data;
	u8* ram;
	usize num_rom_banks;
	usize rom_size;
	usize ram_size;
} Cart;
