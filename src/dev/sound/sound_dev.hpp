#pragma once
#include "types.hpp"
#include "crescent/devlink.h"
#include "dev/dev.hpp"

struct SoundDeviceInfo {
	usize output_count;
};

struct SoundDevice : public Device {
	SoundDevice() {
		name = "sound";
	}

	int handle_request(const kstd::vector<u8>& data, kstd::vector<u8>& res, usize max_size) final;

	virtual int get_info(SoundDeviceInfo& res) = 0;
	virtual int get_output_info(usize index, SoundOutputInfo& res) = 0;

	virtual int set_active_output(void* id) = 0;
	virtual int set_output_params(SoundOutputParams& params) = 0;
	virtual int set_volume(u8 percentage) = 0;
	virtual int queue_output(const void* buffer, usize size) = 0;
	virtual int play(bool play) = 0;
	virtual int reset() = 0;
	virtual int wait_until_consumed(usize trip_size, usize& remaining) = 0;
};
