#pragma once
#include "dev/dev.hpp"
#include "gpu.hpp"

class GpuDevice : public Device {
public:
	explicit GpuDevice(Gpu* gpu, kstd::string_view name) : gpu {gpu} {
		this->name = name;
	}

	int handle_request(const kstd::vector<u8>& data, kstd::vector<u8>& res, usize max_size) override {
		return ERR_UNSUPPORTED;
	}
	Gpu* gpu;
};
