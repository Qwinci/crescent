#include "vga.hpp"
#include "sys.hpp"
#include <cassert>

namespace {
	uint16_t ID {0xB0C5};
	uint16_t X_RES {800};
	uint16_t Y_RES {600};
	uint16_t BPP {32};
	uint16_t ENABLE {0x40 | 1};
	uint16_t BANK {};
	uint16_t VIRT_WIDTH {800};
	uint16_t VIRT_HEIGHT {600};
	uint16_t X_OFFSET {};
	uint16_t Y_OFFSET {};

	uint16_t INDEX {};
}

static uint32_t vbe_read(void*, uint16_t offset, uint8_t size) {
	assert(size == 2);

	if (offset == 1) {
		switch (INDEX) {
			case 0:
				return ID;
			case 1:
				return X_RES;
			case 2:
				return Y_RES;
			case 3:
				return BPP;
			case 4:
				return ENABLE;
			case 5:
				return BANK;
			case 6:
				return VIRT_WIDTH;
			case 7:
				return VIRT_HEIGHT;
			case 8:
				return X_OFFSET;
			case 9:
				return Y_OFFSET;
			case 10:
				return ((800 * 600 * 4 + 0xFFF) & ~0xFFF) / (1024 * 64);
			default:
				break;
		}
	}

	return 0;
}

static void vbe_write(void*, uint16_t offset, uint32_t value, uint8_t size) {
	assert(size == 2);

	if (offset == 0) {
		INDEX = value;
		return;
	}

	printf("dispi write %x %x\n", INDEX, value);

	switch (INDEX) {
		case 0:
			ID = value;
			break;
		case 1:
			X_RES = value;
			break;
		case 2:
			Y_RES = value;
			break;
		case 3:
			BPP = value;
			break;
		case 4:
			ENABLE = value;
			break;
		case 5:
			BANK = value;
			break;
		case 6:
			VIRT_WIDTH = value;
			break;
		case 7:
			VIRT_HEIGHT = value;
			break;
		case 8:
			X_OFFSET = value;
			break;
		case 9:
			Y_OFFSET = value;
			break;
		default:
			break;
	}
}

namespace {
	uint8_t* VRAM;
}

void vga_init(Vm* vm) {
	vm->add_io_region({
		.base = 0x1CE,
		.size = 2,
		.read = vbe_read,
		.write = vbe_write,
		.arg = nullptr
	});

	vm->add_io_region({
		.base = 0x3C6,
		.size = 4,
		.read = [](void*, uint16_t, uint8_t) {
			return 0U;
		},
		.write = [](void*, uint16_t, uint32_t, uint8_t) {},
		.arg = nullptr
	});

	void* vram_ptr = nullptr;
	auto err = sys_map(&vram_ptr, 1024 * 128, CRESCENT_PROT_READ | CRESCENT_PROT_WRITE);
	assert(err == 0);
	memset(vram_ptr, 0, 1024 * 128);

	vm->map_mem(0xA0000, vram_ptr, 1024 * 128);

	VRAM = static_cast<uint8_t*>(vram_ptr);
}

void vga_print_text_mem(uint32_t* fb) {
	// 80x25

	for (uint32_t row = 0; row < 25; ++row) {
		uint32_t start_y = row * 16;

		auto offset = 0x18000 + (row * 80 * 2);
		for (uint32_t col = 0; col < 80; ++col) {
			auto char_byte = VRAM[offset + col * 2];
			auto attribs = VRAM[offset + col * 2 + 1];

			// all chars take 32 bytes
			auto font_c = &VRAM[char_byte * 32];

			// 16x8 font

			uint32_t start_x = col * 8;

			for (uint32_t y = 0; y < 16; ++y) {
				auto byte = font_c[y];
				for (uint32_t x = 0; x < 8; ++x) {
					bool set = byte & 1 << (7 - x);
					fb[(start_y + y) * 800 + (start_x + x)] = set ? 0xFFFFFF : 0;
				}
			}
		}
	}
}
