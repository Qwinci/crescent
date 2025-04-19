#include "chipset.hpp"
#include "pci.hpp"
#include "sys.h"
#include "vga.hpp"
#include <cassert>

namespace {
	bool NMI_DISABLE = false;
	uint8_t CMOS_SELECT = 0xD;
}

extern size_t MEMORY_SIZE;

static uint32_t cmos_read(void* arg, uint16_t offset, uint8_t size) {
	if (offset == 0) {
		assert(size == 1);
		return CMOS_SELECT;
	}
	else {
		assert(offset == 1);
		uint8_t reg = CMOS_SELECT & 0x7F;
		uint8_t value = 0;
		switch (reg) {
			case 0x34:
				value = MEMORY_SIZE >> 16;
				break;
			case 0x35:
				value = MEMORY_SIZE >> 24;
				break;
			default:
				//printf("unsupported cmos register %x\n", reg);
				break;
		}

		CMOS_SELECT = 0xD | (CMOS_SELECT & 1 << 7);
		return value;
	}
}

static void cmos_write(void* arg, uint16_t offset, uint32_t value, uint8_t size) {
	if (offset == 0) {
		assert(size == 1);
		CMOS_SELECT = value;
		NMI_DISABLE = value & 1 << 7;
	}
	else {
		assert(offset == 1);
		uint8_t reg = CMOS_SELECT & 0x7F;
		switch (reg) {
			default:
				printf("unsupported cmos write to register %x\n", reg);
				break;
		}

		CMOS_SELECT = 0xD | (CMOS_SELECT & 1 << 7);
	}
}

namespace {
	uint8_t NMI_STATUS;

	constexpr uint8_t NMI_STATUS_TIM_CNT2_EN = 1 << 0;
	constexpr uint8_t NMI_STATUS_TMR2_OUT_STS = 1 << 5;
}

struct Pit {
	static constexpr uint8_t ACC_LOBYTE_ONLY = 0b1;
	static constexpr uint8_t ACC_HIBYTE_ONLY = 0b10;
	static constexpr uint8_t ACC_LOBYTE_HIBYTE = 0b11;

	struct Channel {
		uint16_t count;
		uint16_t reload_value;
		uint8_t access_mode;
		uint8_t operating_mode;
		bool toggle;
		bool counting;
	};

	Channel channels[3] {};

	static void set_channel_output(uint8_t index, bool signal) {
		if (index == 2) {
			if (signal) {
				NMI_STATUS |= NMI_STATUS_TMR2_OUT_STS;
			}
			else {
				NMI_STATUS &= ~NMI_STATUS_TMR2_OUT_STS;
			}
		}
	}

	static void reload_written(uint8_t index, Channel& channel) {
		channel.count = channel.reload_value;
		channel.counting = true;
	}

	uint32_t read(uint16_t offset, uint8_t size) {
		assert(offset != 3);
		assert(size == 1);

		switch (offset) {
			case 0x0:
			case 0x1:
			case 0x2:
			{
				auto& channel = channels[offset];
				switch (channel.access_mode) {
					case ACC_LOBYTE_ONLY:
						return channel.count & 0xFF;
					case ACC_HIBYTE_ONLY:
						return channel.count >> 8;
					case ACC_LOBYTE_HIBYTE:
						if (channel.toggle) {
							channel.toggle = false;
							return channel.count >> 8;
						}
						else {
							channel.toggle = true;
							return channel.count & 0xFF;
						}
					default:
						return 0;
				}
			}
			default:
				return 0;
		}
	}

	void write(uint16_t offset, uint32_t value, uint8_t size) {
		assert(size == 1);
		assert(value <= 0xFF);
		switch (offset) {
			case 0x0:
			case 0x1:
			case 0x2:
			{
				auto& channel = channels[offset];
				switch (channel.access_mode) {
					case ACC_LOBYTE_ONLY:
						channel.reload_value &= 0xFF00;
						channel.reload_value |= value;
						reload_written(offset, channel);

						switch (channel.operating_mode) {
							case 0:
								set_channel_output(offset, false);
								break;
							default:
								break;
						}

						break;
					case ACC_HIBYTE_ONLY:
						channel.reload_value &= 0xFF;
						channel.reload_value |= value << 8;
						reload_written(offset, channel);

						switch (channel.operating_mode) {
							case 0:
								set_channel_output(offset, false);
								break;
							default:
								break;
						}

						break;
					case ACC_LOBYTE_HIBYTE:
						if (channel.toggle) {
							channel.reload_value &= 0xFF;
							channel.reload_value |= value << 8;
							channel.toggle = false;
							reload_written(offset, channel);
						}
						else {
							channel.reload_value &= 0xFF00;
							channel.reload_value |= value;
							channel.toggle = true;

							switch (channel.operating_mode) {
								case 0:
									set_channel_output(offset, false);
									break;
								default:
									break;
							}
						}
						break;
					default:
						break;
				}

				break;
			}
			case 0x3:
			{
				uint8_t channel_index = value >> 6;
				uint8_t access_mode = value >> 4 & 0b11;
				uint8_t operating_mode = value >> 1 & 0b111;
				bool bcd = value & 1;

				assert(channel_index != 0b11);
				assert(!bcd);

				auto& channel = channels[channel_index];
				channel.access_mode = access_mode;
				channel.operating_mode = operating_mode;
				channel.toggle = false;

				switch (operating_mode) {
					case 0:
						channel.counting = false;
						set_channel_output(channel_index, false);
						break;
					case 2:
						channel.counting = false;
						set_channel_output(channel_index, true);
						break;
					default:
						printf("unsupported pit mode %u\n", operating_mode);
						break;
				}

				break;
			}
			default:
				break;
		}
	}

	void update_channel(uint8_t index) {
		auto& channel = channels[index];
		if (!channel.counting ||
			(index == 2 && !(NMI_STATUS & NMI_STATUS_TIM_CNT2_EN))) {
			return;
		}

		--channel.count;
		switch (channel.operating_mode) {
			case 0:
			{
				if (!channel.count) {
					set_channel_output(index, true);
				}
				break;
			}
			case 2:
			{
				if (channel.count == 1) {
					set_channel_output(index, false);
					channel.count = channel.reload_value;
					set_channel_output(index, true);
				}
				break;
			}
			default:
				break;
		}

	}

	void update() {
		for (uint32_t i = 0; i < 3; ++i) {
			update_channel(i);
		}
	}
};

struct I8042 {
	uint8_t output_buffer[16] {};
	uint8_t input_buffer[2] {};
	uint8_t output_write_ptr {};
	uint8_t output_read_ptr {};
	uint8_t output_size {};
	uint8_t input_size {};

	uint8_t current_cmd {};
	uint8_t current_dev_cmd {};
	uint8_t cmd_args {};
	bool port1_enabled {};
	bool port2_enabled {};

	uint8_t config {};

	bool kb_scanning {};
	bool mouse_scanning {};
	uint8_t scancode_set {};

	void handle_cmd() {
		switch (current_cmd) {
			// write config
			case 0x60:
				config = input_buffer[0];
				input_buffer[0] = input_buffer[1];
				--input_size;
				break;
			// disable second port
			case 0xA7:
				port2_enabled = false;
				break;
			// enable second port
			case 0xA8:
				port2_enabled = true;
				break;
			// test second port
			case 0xA9:
				// test passed
				output_buffer[output_write_ptr++] = 0;
				output_write_ptr %= 16;
				++output_size;
				break;
			// test controller
			case 0xAA:
				// self-test passed
				output_buffer[output_write_ptr++] = 0x55;
				output_write_ptr %= 16;
				++output_size;
				break;
			// test first port
			case 0xAB:
				// test passed
				output_buffer[output_write_ptr++] = 0;
				output_write_ptr %= 16;
				++output_size;
				break;
			// disable first port
			case 0xAD:
				port1_enabled = false;
				break;
			// enable first port
			case 0xAE:
				port1_enabled = true;
				break;
			default:
				printf("ps2 cmd %x\n", current_cmd);
				break;
		}
	}

	void kbd_handle_cmd() {
		switch (current_dev_cmd) {
			// get/set scan code set
			case 0xF0:
			{
				uint8_t value = input_buffer[0];
				input_buffer[0] = input_buffer[1];
				--input_size;

				if (value == 0) {
					if (output_size != 16) {
						output_buffer[output_write_ptr++] = scancode_set;
						output_write_ptr %= 16;
						++output_size;
					}
				}
				else {
					scancode_set = value;
				}

				if (output_size != 16) {
					output_buffer[output_write_ptr++] = 0xFA;
					output_write_ptr %= 16;
					++output_size;
				}

				break;
			}
			// enable scanning
			case 0xF4:
				kb_scanning = true;

				if (output_size == 16) {
					return;
				}

				output_buffer[output_write_ptr++] = 0xFA;
				output_write_ptr %= 16;
				++output_size;
				break;
			// disable scanning
			case 0xF5:
				kb_scanning = false;

				if (output_size == 16) {
					return;
				}

				output_buffer[output_write_ptr++] = 0xFA;
				output_write_ptr %= 16;
				++output_size;
				break;
			// reset
			case 0xFF:
				if (output_size >= 15) {
					return;
				}

				output_buffer[output_write_ptr++] = 0xFA;
				output_write_ptr %= 16;
				output_buffer[output_write_ptr++] = 0xAA;
				output_write_ptr %= 16;
				output_size += 2;
				break;
			default:
				printf("ps2 kb cmd %x\n", current_dev_cmd);
				break;
		}
	}

	uint8_t read(uint16_t offset) {
		switch (offset) {
			case 0:
			{
				// data port
				if (!output_size) {
					return 0;
				}
				auto byte = output_buffer[output_read_ptr++];
				output_read_ptr %= 16;
				--output_size;
				return byte;
			}
			case 4:
				// status port
				return (output_size ? 1 << 0 : 0) |
					(input_size == 2 ? 1 << 1 : 0) |
					(1 << 2);
			default:
				return 0;
		}
	}

	void write(uint16_t offset, uint8_t value) {
		switch (offset) {
			case 0:
				// data port
				if (input_size < 2) {
					input_buffer[input_size++] = value;
				}
				if (current_cmd && input_size >= cmd_args) {
					handle_cmd();
					current_cmd = 0;
				}
				else if (current_dev_cmd && input_size >= cmd_args) {
					kbd_handle_cmd();
					current_dev_cmd = 0;
				}
				else {
					input_buffer[0] = input_buffer[1];
					--input_size;

					if (value == 0xED || value == 0xF0 || value == 0xF3) {
						assert(!current_cmd);
						current_dev_cmd = value;
						cmd_args = 1;

						if (output_size != 16) {
							output_buffer[output_write_ptr++] = 0xFA;
							output_write_ptr %= 16;
							++output_size;
						}
					}
					else {
						assert(!current_cmd);
						current_dev_cmd = value;
						kbd_handle_cmd();
						current_dev_cmd = 0;
					}
				}
				break;
			case 4:
				// command port
				current_cmd = value;

				if (value == 0x60) {
					cmd_args = 1;
				}
				else {
					cmd_args = 0;
					handle_cmd();
				}

				break;
			default:
				break;
		}
	}
};

static Pit PIT {};
static I8042 I8042 {};

void chipset_init(Vm* vm, uint32_t* fb, uint32_t fb_size) {
	vm->add_io_region({
		.base = 0x40,
		.size = 4,
		.read = [](void*, uint16_t offset, uint8_t size) {
			return PIT.read(offset, size);
		},
		.write = [](void*, uint16_t offset, uint32_t value, uint8_t size) {
			PIT.write(offset, value, size);
		},
		.arg = nullptr
	});

	vm->add_io_region({
		.base = 0x60,
		.size = 1,
		.read = [](void*, uint16_t, uint8_t size) {
			assert(size == 1);
			return static_cast<uint32_t>(I8042.read(0));
		},
		.write = [](void*, uint16_t, uint32_t value, uint8_t size) {
			assert(size == 1);
			I8042.write(0, value);
		},
		.arg = nullptr
	});

	vm->add_io_region({
		.base = 0x61,
		.size = 1,
		.read = [](void*, uint16_t, uint8_t size) {
			assert(size == 1);
			return static_cast<uint32_t>(NMI_STATUS);
		},
		.write = [](void*, uint16_t, uint32_t value, uint8_t size) {
			assert(size == 1);
			NMI_STATUS = value;
		},
		.arg = nullptr
	});

	vm->add_io_region({
		.base = 0x64,
		.size = 1,
		.read = [](void*, uint16_t, uint8_t size) {
			assert(size == 1);
			return static_cast<uint32_t>(I8042.read(4));
		},
		.write = [](void*, uint16_t, uint32_t value, uint8_t size) {
			assert(size == 1);
			I8042.write(4, value);
		},
		.arg = nullptr
	});

	vm->add_io_region({
		.base = 0x70,
		.size = 2,
		.read = cmos_read,
		.write = cmos_write,
		.arg = nullptr
	});

	// fast a20
	vm->add_io_region({
		.base = 0x92,
		.size = 1,
		.read = [](void*, uint16_t, uint8_t size) {
			assert(size == 1);
			return uint32_t {0};
		},
		.write = [](void*, uint16_t offset, uint32_t value, uint8_t size) {
			assert(size == 1);
		},
		.arg = nullptr
	});

	// vga
	vm->add_io_region({
		.base = 0x3C0,
		.size = 21,
		.read = [](void*, uint16_t, uint8_t) {
			return 0U;
		},
		.write = [](void*, uint16_t, uint32_t, uint8_t) {

		},
		.arg = nullptr
	});

	uint8_t max_bus = 0;
	uint8_t max_dev = 1;
	size_t pci_ecam_size = (max_bus << 20 | max_dev << 15) + 0x1000;

	vm->add_mmio_region({
		.base = 0xB0000000,
		.size = pci_ecam_size,
		.read = [](void*, uint64_t offset, uint8_t size) {
			auto pci_offset = offset & 0xFFF;
			PciAddress addr {
				.bus = static_cast<uint8_t>(offset >> 20),
				.dev = static_cast<uint8_t>(offset >> 15 & 0b11111),
				.func = static_cast<uint8_t>(offset >> 12 & 0b111),
			};
			return static_cast<uint64_t>(pci_config_read(addr, pci_offset, size));
		},
		.write = [](void*, uint64_t offset, uint64_t value, uint8_t size) {
			auto pci_offset = offset & 0xFFF;
			PciAddress addr {
				.bus = static_cast<uint8_t>(offset >> 20),
				.dev = static_cast<uint8_t>(offset >> 15 & 0b11111),
				.func = static_cast<uint8_t>(offset >> 12 & 0b111),
			};
			pci_config_write(addr, pci_offset, value, size);
		},
		.arg = nullptr
	});

	auto device = std::make_unique<PciDevice>(PciAddress {0, 0, 0}, true);
	device->config.set_device(0x8086, 0x29C0);
	device->config.set_subsystem(0x1AF4, 0x1100);
	device->config.set_type(0);

	pci_add_device(std::move(device));

	auto vga_device = std::make_unique<PciDevice>(PciAddress {0, 1, 0}, true);
	vga_device->config.set_device(0x1234, 0x1111);
	vga_device->config.set_class(3, 0, 0, 0);
	vga_device->config.set_type(0);

	CrescentHandle vga_rom_handle;
	auto err = sys_open(&vga_rom_handle, "/vgabios.bin", sizeof("/vgabios.bin") - 1, 0);
	assert(err == 0);

	CrescentStat bios_stat {};
	err = sys_stat(vga_rom_handle, &bios_stat);
	assert(err == 0);

	void* vga_rom_mem = nullptr;
	err = sys_map(&vga_rom_mem, (bios_stat.size + 0xFFF) & ~0xFFF, CRESCENT_PROT_READ | CRESCENT_PROT_WRITE);
	assert(err == 0);
	err = sys_read(vga_rom_handle, vga_rom_mem, bios_stat.size, nullptr);
	assert(err == 0);
	sys_close_handle(vga_rom_handle);

	vga_device->config.bars[0] = fb;
	vga_device->config.bar_sizes[0] = fb_size;
	vga_device->config.bars[6] = vga_rom_mem;
	vga_device->config.bar_sizes[6] = (bios_stat.size + 0xFFF) & ~0xFFF;

	pci_add_device(std::move(vga_device));

	vga_init(vm);
}

static uint64_t remainder = 0;

void chipset_update_timers(uint64_t elapsed_tsc, const ArchInfo& info) {
	const uint64_t tsc_per_pit = info.tsc_frequency / 1193182;

	auto pit_ticks = (elapsed_tsc + remainder) / tsc_per_pit;
	for (uint64_t i = 0; i < pit_ticks; ++i) {
		PIT.update();
	}

	remainder = (elapsed_tsc + remainder) % tsc_per_pit;
}
