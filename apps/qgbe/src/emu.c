#include "emu.h"
#include "mbc/mbc1.h"
#include "mbc/no_mbc.h"
#include "utils/fsize.h"
#include <stdio.h>
#include <stdlib.h>

Emulator* emu_new() {
	Emulator* emu = calloc(1, sizeof(Emulator));
	emu->bus.cpu.bus = &emu->bus;
	emu->bus.ppu.bus = &emu->bus;
	emu->bus.timer.bus = &emu->bus;
	emu->bus.joyp = 0xFF;
	ppu_reset(&emu->bus.ppu);
	return emu;
}

bool emu_load_boot_rom(Emulator* self, const char* path) {
	usize size = fsize(path);
	if (!size || size > 0x100) {
		return false;
	}

	FILE* file = fopen(path, "rb");
	fread(&self->bus.boot_rom, size, 1, file);
	fclose(file);

	self->bus.bootrom_mapped = true;

	return true;
}

bool emu_load_rom(Emulator* self, const char* path) {
	usize size = fsize(path);
	if (!size) {
		return false;
	}

	self->bus.cart.data = malloc(size);
	if (!self->bus.cart.data) {
		return false;
	}

	FILE* file = fopen(path, "rb");
	fread(self->bus.cart.data, size, 1, file);
	fclose(file);

	const CartHdr* hdr = (const CartHdr*) (self->bus.cart.data + 0x100);

	usize rom_banks = 2 << hdr->rom_size;
	usize rom_size = (1024 * 32) * (1 << hdr->rom_size);
	usize ram_size = 0;
	if (hdr->ram_size == 1) {
		ram_size = 1024;
	}
	else if (hdr->ram_size == 2) {
		ram_size = 1024 * 8;
	}
	else if (hdr->ram_size == 3) {
		ram_size = 1024 * 32;
	}
	else if (hdr->ram_size == 4) {
		ram_size = 1024 * 128;
	}
	else if (hdr->ram_size == 5) {
		ram_size = 1024 * 64;
	}

	if (ram_size) {
		self->bus.cart.ram = malloc(ram_size);
		if (!self->bus.cart.ram) {
			return false;
		}
	}

	self->bus.cart.rom_size = rom_size;
	self->bus.cart.ram_size = ram_size;
	self->bus.cart.num_rom_banks = rom_banks;
	self->bus.cart.hdr = hdr;

	Mapper* mapper = NULL;
	// ROM ONLY
	if (hdr->type == 0) {
		mapper = no_mbc_new(&self->bus.cart);
	}
	// MBC1/MBC1+RAM/MBC1+RAM+BATTERY
	else if (hdr->type == 1 || hdr->type == 2 || hdr->type == 3) {
		mapper = mbc1_new(&self->bus.cart);
	}
	else if (hdr->type == 0x13) {
		fprintf(stderr, "warning: using mbc1 for mbc3 rom\n");
		mapper = mbc1_new(&self->bus.cart);
	}
	// MBC3+TIMER+BATTERY/MBC3+TIMER+RAM+BATTERY/MBC3/MBC3+RAM/MBC3+RAM+BATTERY
	else {
		fprintf(stderr, "error: unsupported cartridge type %X\n", hdr->type);
		exit(1);
	}

	if (!mapper) {
		return false;
	}
	self->bus.cart.mapper = mapper;

	return true;
}

#include <assert.h>
#include <sys.h>
#include <windower/windower.h>

#define REAL_WIDTH 160
#define REAL_HEIGHT 144

#define TILE_VIEWER_WIDTH (128 * 4)
#define TILE_VIEWER_HEIGHT (128 * 4)

#define SPRITE_VIEWER_WIDTH (128 * 4)
#define SPRITE_VIEWER_HEIGHT (128 * 4)

void emu_run(Emulator* self) {
	// A: 01 F: B0 B: 00 C: 13 D: 00 E: D8 H: 01 L: 4D SP: FFFE PC: 00:0100 (00 C3 13 02)
	if (!self->bus.bootrom_mapped) {
		Cpu* cpu = &self->bus.cpu;
		cpu->regs[REG_A] = 1;
		cpu->regs[REG_F] = 0xB0;
		cpu->regs[REG_B] = 0;
		cpu->regs[REG_C] = 0x13;
		cpu->regs[REG_D] = 0;
		cpu->regs[REG_E] = 0xD8;
		cpu->regs[REG_H] = 1;
		cpu->regs[REG_L] = 0x4D;
		cpu->sp = 0xFFFE;
		cpu->pc = 0x100;
		self->bus.ppu.lcdc |= 1 << 7;
	}

	Windower windower = {};
	int status = windower_connect(&windower);
	assert(status == 0);

	WindowerWindow window = {};
	status = windower_create_window(
		&windower,
		&window,
		0,
		0,
		REAL_WIDTH * 6,
		REAL_HEIGHT * 6);
	assert(status == 0);

	status = windower_window_map_fb(&window);
	assert(status == 0);

	u32* fb = (u32*) windower_window_get_fb_mapping(&window);

	u32* backing = (u32*) calloc(1, REAL_WIDTH * REAL_HEIGHT * 4);

	self->bus.ppu.texture = backing;

	Scancode key_down = SCANCODE_DOWN;
	Scancode key_up = SCANCODE_UP;
	Scancode key_left = SCANCODE_LEFT;
	Scancode key_right = SCANCODE_RIGHT;

	Scancode key_start = SCANCODE_RETURN;
	Scancode key_select = SCANCODE_BACKSPACE;
	Scancode key_b = SCANCODE_B;
	Scancode key_a = SCANCODE_A;

	DevLinkRequest sound_req = {
		.type = DevLinkRequestGetDevices,
		.size = sizeof(DevLinkRequest),
		.handle = INVALID_CRESCENT_HANDLE,
		.data = {
			.get_devices = {
				.type = CrescentDeviceTypeSound
			}
		}
	};
	char response[DEVLINK_BUFFER_SIZE];
	DevLink dev_link = {
		.request = &sound_req,
		.response_buffer = response,
		.response_buf_size = DEVLINK_BUFFER_SIZE
	};
	status = sys_devlink(&dev_link);
	CrescentHandle sound_handle = INVALID_CRESCENT_HANDLE;
	if (status == 0 && dev_link.response->get_devices.device_count) {
		sound_req.type = DevLinkRequestOpenDevice;
		sound_req.data.open_device.device = dev_link.response->get_devices.names[0];
		sound_req.data.open_device.device_len = dev_link.response->get_devices.name_sizes[0];
		status = sys_devlink(&dev_link);
		if (status == 0) {
			sound_handle = dev_link.response->open_device.handle;
		}
		else {
			puts("failed to open sound device");
		}
	}
	else {
		puts("failed to get sound devices");
	}

	bool use_sound = false;
	SoundLink sound_link = SOUND_LINK_INITIALIZER;
	if (sound_handle != INVALID_CRESCENT_HANDLE) {
		sound_link.op = SoundLinkOpGetInfo;
		dev_link.request = &sound_link.request;
		sound_link.request.handle = sound_handle;
		status = sys_devlink(&dev_link);
		if (status != 0) {
			puts("failed to get sound info");
			goto skip_sound;
		}
		SoundLinkResponse* sound_resp = (SoundLinkResponse*) dev_link.response->specific;
		printf("%d outputs\n", (int) sound_resp->info.output_count);
		if (sound_resp->info.output_count == 0) {
			goto skip_sound;
		}

		sound_link.op = SoundLinkOpGetOutputInfo;
		// laptop index = 4
		sound_link.get_output_info.index = 0;
		status = sys_devlink(&dev_link);
		if (status != 0) {
			puts("failed to get output info");
			goto skip_sound;
		}

		size_t buffer_size = sound_resp->output_info.buffer_size;

		sound_link.op = SoundLinkOpSetActiveOutput;
		sound_link.set_active_output.id = sound_resp->output_info.id;
		status = sys_devlink(&dev_link);
		if (status != 0) {
			puts("failed to set active output");
			goto skip_sound;
		}

		sound_link.op = SoundLinkOpSetOutputParams;
		sound_link.set_output_params.params = (SoundOutputParams) {
			.sample_rate = 48000,
			.channels = 2,
			.fmt = SoundFormatPcmU16
		};
		status = sys_devlink(&dev_link);
		if (status != 0) {
			puts("failed to set output params");
			goto skip_sound;
		}

		sound_link.op = SoundLinkOpPlay;
		sound_link.play.play = true;
		status = sys_devlink(&dev_link);
		if (status != 0) {
			puts("failed to play sound");
			goto skip_sound;
		}
	}

	use_sound = true;
skip_sound:

	f32 audio_buffer[2048] = {};
	uint16_t pcm_audio_buffer[2048] = {};
	usize audio_size = 0;
	/*SDL_AudioDeviceID audio_dev = SDL_OpenAudioDevice(NULL, false, &spec, &spec, 0);
	if (!audio_dev) {
		fprintf(stderr, "failed to open audio device: %s\n", SDL_GetError());
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return;
	}
	SDL_PauseAudioDevice(audio_dev, false);*/

	bool running = true;
	f64 delta;
	uint64_t last_time;
	status = sys_get_time(&last_time);
	assert(status == 0);
	uint64_t perf_freq = 1000UL * 1000UL * 1000UL;

	bool key_state[SCANCODE_MAX] = {};

	while (running) {
		WindowerProtocolWindowEvent event = {};
		while (windower_window_poll_event(&window, &event) == 0) {
			if (event.type == WindowerProtocolWindowEventCloseRequested) {
				running = false;
			}
			else if (event.type == WindowerProtocolWindowEventKey) {
				key_state[event.key.code] = event.key.pressed;
			}
		}

		u8* joyp = &self->bus.joyp;

		// Actions
		if (*joyp & 1 << 5) {
			if (key_state[key_start]) {
				*joyp &= ~(1 << 3);
			}
			else {
				*joyp |= 1 << 3;
			}

			if (key_state[key_select]) {
				*joyp &= ~(1 << 2);
			}
			else {
				*joyp |= 1 << 2;
			}

			if (key_state[key_b]) {
				*joyp &= ~(1 << 1);
			}
			else {
				*joyp |= 1 << 1;
			}

			if (key_state[key_a]) {
				*joyp &= ~(1 << 0);
			}
			else {
				*joyp |= 1 << 0;
			}
		}
		if (*joyp & 1 << 4) {
			if (key_state[key_down]) {
				*joyp &= ~(1 << 3);
			}
			else {
				*joyp |= 1 << 3;
			}

			if (key_state[key_up]) {
				*joyp &= ~(1 << 2);
			}
			else {
				*joyp |= 1 << 2;
			}

			if (key_state[key_left]) {
				*joyp &= ~(1 << 1);
			}
			else {
				*joyp |= 1 << 1;
			}

			if (key_state[key_right]) {
				*joyp &= ~(1 << 0);
			}
			else {
				*joyp |= 1 << 0;
			}
		}

		usize cycles = 0;
		while (!self->bus.ppu.frame_ready) {
			bus_cycle(&self->bus);
			if (cycles % 22 == 0) {
				apu_gen_sample(&self->bus.apu, audio_buffer + audio_size);
				audio_size += 2;
				if (audio_size == sizeof(audio_buffer) / sizeof(*audio_buffer)) {
					if (use_sound) {
						for (usize i = 0; i < sizeof(audio_buffer) / sizeof(*audio_buffer); ++i) {
							pcm_audio_buffer[i] = (uint16_t) (audio_buffer[i] * 32767);
						}

						sound_link.op = SoundLinkOpQueueOutput;
						sound_link.queue_output.buffer = pcm_audio_buffer;
						sound_link.queue_output.len = sizeof(pcm_audio_buffer);
						status = sys_devlink(&dev_link);
						if (status != 0) {
							puts("failed to queue audio");
						}
					}

					audio_size = 0;
				}
			}
			cycles += 1;
		}

		self->bus.ppu.frame_ready = false;

		for (u32 y = 0; y < REAL_HEIGHT * 6; ++y) {
			for (u32 x = 0; x < REAL_WIDTH * 6; ++x) {
				fb[y * REAL_WIDTH * 6 + x] = backing[(y / 6) * REAL_WIDTH + x / 6];
			}
		}

		windower_window_redraw(&window);

		uint64_t cur_time;
		status = sys_get_time(&cur_time);
		assert(status == 0);

		uint64_t delta_ticks = cur_time - last_time;
		last_time = cur_time;
		const f64 wanted_delta = 1.0 / 60.0f;
		delta = (f64) delta_ticks / (f64) perf_freq;
		//fprintf(stderr, "delta: %f, fps: %d\n", delta, (int) (1.0f / delta));
		if (delta < wanted_delta) {
			sys_sleep((uint64_t) ((wanted_delta - delta) * 1000UL * 1000UL * 1000UL));
		}
	}

	windower_window_destroy(&window);
	windower_destroy(&windower);

	if (sound_handle != INVALID_CRESCENT_HANDLE) {
		sys_close_handle(sound_handle);
	}

	if (self->bus.cart.data) {
		free(self->bus.cart.data);
	}
	if (self->bus.cart.ram) {
		free(self->bus.cart.ram);
	}
}
