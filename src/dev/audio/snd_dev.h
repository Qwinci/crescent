#pragma once
#include "sys/dev.h"

typedef enum {
	SND_PCM_INVALID,
	SND_PCM_8,
	SND_PCM_16NE
} AudioFormat;

typedef struct {
	u32 sample_rate;
	AudioFormat format;
	u32 channels;
	u32 preferred_buffer_size;
} AudioParams;

typedef struct {
	u8* buffer;
	u8* write_ptr;
	u32 buffer_size;
	u32 id;
	bool write_to_end;
} AudioStream;

typedef struct SndDev {
	GenericDevice generic;
	void (*user_callback)(void* ptr, size_t len, void* user_data);
	void* user_data;
	bool (*create_stream)(struct SndDev* self, AudioParams* params, AudioStream* res);
	bool (*destroy_stream)(struct SndDev* self, AudioStream* stream);
	bool (*play)(struct SndDev* self, AudioStream* stream, bool play);
	bool (*write)(struct SndDev* self, AudioStream* stream, const void* data, usize len);
} SndDev;
