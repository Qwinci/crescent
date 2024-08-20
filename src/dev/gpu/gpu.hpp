#pragma once
#include "types.hpp"

struct GpuSurface {
	virtual ~GpuSurface() = default;
	virtual usize get_phys() = 0;
};

class Gpu {
public:
	virtual GpuSurface* create_surface() = 0;
	virtual void destroy_surface(GpuSurface* surface) = 0;
	virtual void flip(GpuSurface* surface) = 0;
	bool owns_boot_fb {};
	bool supports_page_flipping {};
};
