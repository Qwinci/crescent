#pragma once
#include "types.h"

typedef struct {
	u16 cycle_rate;
	u16 cur_cycle;
	u8 cur_duty;
	u8 duty_cycle;
	u8 len_timer;
} SquareWaveGenerator;

typedef struct {
	u16 cycle_rate;
	u16 cur_cycle;
	u16 len_timer;
	u8 cur_wave;
} WaveGenerator;

typedef struct {
	u8 len_timer;
} NoiseGenerator;

typedef struct {
	u8 init_vol;
	bool increase;
	u8 sweep_pace;
	u8 vol;
	u8 cycle;
} VolumeEnvelopeGenerator;

typedef struct {
	SquareWaveGenerator channel1_generator;
	SquareWaveGenerator channel2_generator;
	WaveGenerator channel3_generator;
	NoiseGenerator channel4_generator;
	VolumeEnvelopeGenerator channel1_vol_envelope;
	VolumeEnvelopeGenerator channel2_vol_envelope;
	VolumeEnvelopeGenerator channel4_vol_envelope;

	u8 ch1_freq_sweep_pace;
	bool ch1_freq_decrease;
	u8 ch1_freq_sweep_slope;
	u8 ch1_freq_cycle;
	bool ch1_freq_pace_reload;

	u8 clock;
	u8 nr10;
	u8 nr11;
	u8 nr12;
	u8 nr13;
	u8 nr14;
	u8 nr21;
	u8 nr22;
	u8 nr23;
	u8 nr24;
	u8 nr30;
	u8 nr31;
	u8 nr32;
	u8 nr33;
	u8 nr34;
	u8 nr41;
	u8 nr42;
	u8 nr43;
	u8 nr44;
	u8 nr50;
	u8 nr51;
	u8 nr52;
	bool dac_enable[4];
	u8 wave_pattern[16];
	u8 wave_sample;
	bool enabled;
} Apu;

void apu_clock(Apu* self);
void apu_clock_channels(Apu* self);
void apu_gen_sample(Apu* self, f32 out[2]);
void apu_write(Apu* self, u16 addr, u8 value);
