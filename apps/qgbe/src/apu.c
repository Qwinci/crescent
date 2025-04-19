#include "apu.h"
#include <string.h>

static void volume_envelope_reload(VolumeEnvelopeGenerator* self, u8 nr_2) {
	self->init_vol = nr_2 >> 4;
	self->increase = nr_2 & 1 << 3;
	self->sweep_pace = nr_2 & 0b111;
	self->cycle = 0;
	self->vol = self->init_vol;
}

#define NR51_CH1_RIGHT (1 << 0)
#define NR51_CH2_RIGHT (1 << 1)
#define NR51_CH3_RIGHT (1 << 2)
#define NR51_CH4_RIGHT (1 << 3)
#define NR51_CH1_LEFT (1 << 4)
#define NR51_CH2_LEFT (1 << 5)
#define NR51_CH3_LEFT (1 << 6)
#define NR51_CH4_LEFT (1 << 7)

#define NR52_CH1_ON (1 << 0)
#define NR52_CH2_ON (1 << 1)
#define NR52_CH3_ON (1 << 2)
#define NR52_CH4_ON (1 << 3)

void apu_write(Apu* self, u16 addr, u8 value) {
	if (addr >= 0xFF30 && addr <= 0xFF3F) {
		self->wave_pattern[addr - 0xFF30] = value;
		return;
	}
	if ((addr != 0xFF26) & !self->enabled) {
		return;
	}

	if (addr == 0xFF10) {
		self->nr10 = value;
		self->ch1_freq_pace_reload = true;
		self->ch1_freq_decrease = value & 1 << 3;
		self->ch1_freq_sweep_slope = value & 0b111;
	}
	else if (addr == 0xFF11) {
		self->nr11 = value;
		self->channel1_generator.duty_cycle = value >> 6;
		self->channel1_generator.len_timer = value & 0b111111;
	}
	else if (addr == 0xFF12) {
		self->nr12 = value;
		if (!(value & 0xF8)) {
			self->nr52 &= ~NR52_CH1_ON;
			self->dac_enable[0] = false;
		}
		else {
			self->dac_enable[0] = true;
		}
	}
	else if (addr == 0xFF13) {
		self->nr13 = value;
		self->channel1_generator.cycle_rate = (2048 - (value | ((self->nr14 & 0b111) << 8))) * 4;
	}
	else if (addr == 0xFF14) {
		self->nr14 = value;
		self->channel1_generator.cycle_rate = (2048 - (self->nr13 | ((value & 0b111) << 8))) * 4;
		if (self->dac_enable[0] && value & 1 << 7) {
			volume_envelope_reload(&self->channel1_vol_envelope, self->nr12);
			self->nr52 |= NR52_CH1_ON;
			if (self->channel1_generator.len_timer == 64) {
				self->channel1_generator.len_timer = 0;
			}
		}
	}
	else if (addr == 0xFF16) {
		self->nr21 = value;
		self->channel2_generator.duty_cycle = value >> 6;
		self->channel2_generator.len_timer = value & 0b111111;
	}
	else if (addr == 0xFF17) {
		self->nr22 = value;
		if (!(value & 0xF8)) {
			self->nr52 &= ~NR52_CH2_ON;
			self->dac_enable[1] = false;
		}
		else {
			self->dac_enable[1] = true;
		}
	}
	else if (addr == 0xFF18) {
		self->nr23 = value;
		self->channel2_generator.cycle_rate = (2048 - (value | ((self->nr24 & 0b111) << 8))) * 4;
	}
	else if (addr == 0xFF19) {
		self->nr24 = value;
		self->channel2_generator.cycle_rate = (2048 - (self->nr23 | ((value & 0b111) << 8))) * 4;
		if (self->dac_enable[1] && value & 1 << 7) {
			volume_envelope_reload(&self->channel2_vol_envelope, self->nr22);
			self->nr52 |= NR52_CH2_ON;
			if (self->channel2_generator.len_timer == 64) {
				self->channel2_generator.len_timer = 0;
			}
		}
	}
	else if (addr == 0xFF1A) {
		self->nr30 = value;
		if (value & 1 << 7) {
			self->dac_enable[2] = true;
		}
		else {
			self->nr52 &= ~NR52_CH3_ON;
			self->dac_enable[2] = false;
		}
	}
	else if (addr == 0xFF1B) {
		self->nr31 = value;
		self->channel3_generator.len_timer = value;
	}
	else if (addr == 0xFF1C) {
		self->nr32 = value;
	}
	else if (addr == 0xFF1D) {
		self->nr33 = value;
		self->channel3_generator.cycle_rate = (2048 - (value | (self->nr34 & 0b111) << 8)) * 2;
	}
	else if (addr == 0xFF1E) {
		self->nr34 = value;
		self->channel3_generator.cycle_rate = (2048 - (self->nr33 | (value & 0b111) << 8)) * 2;
		if (self->dac_enable[2] && value & 1 << 7) {
			self->nr52 |= NR52_CH3_ON;
			self->channel3_generator.cur_wave = 0;
			self->channel3_generator.cur_cycle = 0;
			if (self->channel3_generator.len_timer == 256) {
				self->channel3_generator.len_timer = 0;
			}
		}
	}
	else if (addr == 0xFF20) {
		self->nr41 = value;
		self->channel4_generator.len_timer = value & 0b111111;
	}
	else if (addr == 0xFF21) {
		self->nr42 = value;
		if (!(value & 0xF8)) {
			self->nr52 &= ~NR52_CH4_ON;
			self->dac_enable[3] = false;
		}
		else {
			self->dac_enable[3] = true;
		}
	}
	else if (addr == 0xFF22) {
		self->nr43 = value;
	}
	else if (addr == 0xFF23) {
		self->nr44 = value;
		if (self->dac_enable[3] && value & 1 << 7) {
			volume_envelope_reload(&self->channel4_vol_envelope, self->nr42);
			self->nr52 |= NR52_CH4_ON;
			if (self->channel4_generator.len_timer == 64) {
				self->channel4_generator.len_timer = 0;
			}
		}
	}
	else if (addr == 0xFF24) {
		self->nr50 = value;
	}
	else if (addr == 0xFF25) {
		self->nr51 = value;
	}
	else if (addr == 0xFF26) {
		if (!(value & 1 << 7)) {
			u8 saved[16];
			memcpy(saved, self->wave_pattern, 16);
			memset(self, 0, sizeof(Apu));
			self->enabled = false;
			memcpy(self->wave_pattern, saved, 16);
		}
		else {
			self->enabled = true;
			self->nr52 = 1 << 7;
			if (self->nr12 & 0xF8) {
				self->nr52 |= NR52_CH1_ON;
			}
			if (self->nr22 & 0xF8) {
				self->nr52 |= NR52_CH2_ON;
			}
			if (self->nr32 & 0xF8) {
				self->nr52 |= NR52_CH3_ON;
			}
			if (self->nr42 & 0xF8) {
				self->nr42 |= NR52_CH4_ON;
			}
		}
	}
}

void apu_clock_channels(Apu* self) {
	if (!self->enabled) {
		return;
	}

	if ((self->nr52 & NR52_CH1_ON) && ++self->channel1_generator.cur_cycle == self->channel1_generator.cycle_rate) {
		self->channel1_generator.cur_duty = (self->channel1_generator.cur_duty + 1) & 7;
		self->channel1_generator.cur_cycle = 0;
	}

	if ((self->nr52 & NR52_CH2_ON) && ++self->channel2_generator.cur_cycle == self->channel2_generator.cycle_rate) {
		self->channel2_generator.cur_duty = (self->channel2_generator.cur_duty + 1) & 7;
		self->channel2_generator.cur_cycle = 0;
	}

	if ((self->nr52 & NR52_CH3_ON) && ++self->channel3_generator.cur_cycle == self->channel3_generator.cycle_rate) {
		self->channel3_generator.cur_wave = (self->channel3_generator.cur_wave + 1) % 32;
		self->wave_sample = self->wave_pattern[self->channel3_generator.cur_wave / 2];
		self->channel3_generator.cur_cycle = 0;
	}
}

void apu_clock(Apu* self) {
	if (!self->enabled) {
		return;
	}

	// Envelope sweep
	if (self->clock % 8 == 0) {
		VolumeEnvelopeGenerator* env1 = &self->channel2_vol_envelope;
		if (env1->sweep_pace && ++env1->cycle % env1->sweep_pace == 0) {
			if (env1->increase) {
				if (env1->vol < 15) {
					env1->vol += 1;
				}
			}
			else {
				if (env1->vol > 0) {
					env1->vol -= 1;
				}
			}
		}

		VolumeEnvelopeGenerator* env2 = &self->channel2_vol_envelope;
		if (env2->sweep_pace && ++env2->cycle % env2->sweep_pace == 0) {
			if (env2->increase) {
				if (env2->vol < 15) {
					env2->vol += 1;
				}
			}
			else {
				if (env2->vol > 0) {
					env2->vol -= 1;
				}
			}
		}
	}

	// Sound length
	if (self->clock % 2 == 0) {
		if (self->nr14 & 1 << 6 && self->channel1_generator.len_timer < 64) {
			self->channel1_generator.len_timer += 1;
			if (self->channel1_generator.len_timer == 64) {
				self->nr52 &= ~NR52_CH1_ON;
			}
		}

		if (self->nr24 & 1 << 6 && self->channel2_generator.len_timer < 64) {
			self->channel2_generator.len_timer += 1;
			if (self->channel2_generator.len_timer == 64) {
				self->nr52 &= ~NR52_CH2_ON;
			}
		}

		if (self->nr34 & 1 << 6 && self->channel3_generator.len_timer < 256) {
			self->channel3_generator.len_timer += 1;
			if (self->channel3_generator.len_timer == 256) {
				self->nr52 &= ~NR52_CH3_ON;
			}
		}

		if (self->nr44 & 1 << 6 && self->channel4_generator.len_timer < 64) {
			self->channel4_generator.len_timer += 1;
			if (self->channel4_generator.len_timer == 64) {
				self->nr52 &= ~NR52_CH4_ON;
			}
		}
	}

	// CH1 freq sweep
	if (self->clock % 4 == 0) {
		if (self->nr52 & NR52_CH1_ON && ++self->ch1_freq_cycle == self->ch1_freq_sweep_pace) {
			u16 wavelength = 2048 - (self->nr13 | ((self->nr14 & 0b111) << 8));
			if (self->ch1_freq_sweep_slope) {
				if (self->ch1_freq_decrease) {
					wavelength -= wavelength / (2 * self->ch1_freq_sweep_slope);
					wavelength &= 0x7FF;
					self->nr13 = wavelength;
					self->nr14 &= ~0b111;
					self->nr14 |= wavelength >> 8;
				}
				else {
					wavelength += wavelength / (2 * self->ch1_freq_sweep_slope);
					if (wavelength > 0x7FF) {
						self->nr52 &= ~NR52_CH1_ON;
					}
					else {
						self->nr13 = wavelength;
						self->nr14 &= ~0b111;
						self->nr14 |= wavelength >> 8;
					}
				}

				self->channel1_generator.cycle_rate = (2048 - (self->nr13 | ((self->nr14 & 0b111) << 8))) * 4;
			}

			if (self->ch1_freq_pace_reload) {
				self->ch1_freq_sweep_pace = self->nr10 >> 4;
				self->ch1_freq_pace_reload = false;
			}
			self->ch1_freq_cycle = 0;
		}
	}

	self->clock += 1;
}

static bool DUTY_TABLE[][8] = {
	[0] = {true, true, true, true, true, true, true, false},
	[1] = {false, true, true, true, true, true, true, false},
	[2] = {false, true, true, true, true, false, false, false},
	[3] = {true, false, false, false, false, false, false, true}
};

static f32 left_capacitor = 0;
f32 do_high_pass_left(f32 in) {
	f32 out = in - left_capacitor;
	left_capacitor = in - out * 0.999958f;
	return out;
}

static f32 right_capacitor = 0;
f32 do_high_pass_right(f32 in) {
	f32 out = in - right_capacitor;
	right_capacitor = in - out * 0.999958f;
	return out;
}

void apu_gen_sample(Apu* self, f32 out[2]) {
	f32 left = 0;
	f32 right = 0;

	if (self->nr52 & NR52_CH1_ON) {
		u8 vol = self->channel1_vol_envelope.vol;
		if (vol && DUTY_TABLE[self->channel1_generator.duty_cycle][self->channel1_generator.cur_duty]) {
			f32 amp = 2.0f / 15.0f * (f32) vol;
			if (amp < 1) {
				amp *= -1;
			}
			else {
				amp -= 1;
			}
			if (self->nr51 & NR51_CH1_LEFT) {
				left += amp;
			}
			if (self->nr51 & NR51_CH1_RIGHT) {
				right += amp;
			}
		}
	}
	if (self->nr52 & NR52_CH2_ON) {
		u8 vol = self->channel2_vol_envelope.vol;
		if (vol && DUTY_TABLE[self->channel2_generator.duty_cycle][self->channel2_generator.cur_duty]) {
			f32 amp = 2.0f / 15.0f * (f32) vol;
			if (amp < 1) {
				amp *= -1;
			}
			else {
				amp -= 1;
			}
			if (self->nr51 & NR51_CH2_LEFT) {
				left += amp;
			}
			if (self->nr51 & NR51_CH2_RIGHT) {
				right += amp;
			}
		}
	}

	if (self->nr52 & NR52_CH3_ON) {
		u8 vol = self->nr32 >> 5 & 0b11;
		u8 sample = self->wave_sample >> (!(self->channel3_generator.cur_wave & 1) * 4) & 0b1111;
		if (vol == 0b10) {
			sample >>= 1;
		}
		else if (vol == 0b11) {
			sample >>= 2;
		}
		if (vol && sample) {
			//f32 amp = 0.5f / 15.0f * (f32) sample;
			f32 f_sample = 2.0f / 15.0f * (f32) sample;
			if (f_sample < 1) {
				f_sample *= -1;
			}
			else {
				f_sample -= 1;
			}
			if (self->nr51 & NR51_CH3_LEFT) {
				left += f_sample;
			}
			if (self->nr51 & NR51_CH3_RIGHT) {
				right += f_sample;
			}
		}
	}

	u8 left_vol = (self->nr50 >> 4 & 0b111) + 1;
	u8 right_vol = (self->nr50 & 0b111) + 1;
	f32 left_amp = 0.8f / 8.0f * (f32) left_vol;
	f32 right_amp = 0.8f / 8.0f * (f32) right_vol;

	out[0] = do_high_pass_left(left_amp * left);
	out[1] = do_high_pass_right(right_amp * right);
}
