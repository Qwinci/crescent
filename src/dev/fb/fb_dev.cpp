#include "dev/gpu/gpu_dev.hpp"
#include "dev/user_dev.hpp"
#include "fb.hpp"
#include "sched/process.hpp"
#include "sched/sched.hpp"

struct GpuFbDev : public UserDevice {
	explicit GpuFbDev(Framebuffer* fb, Gpu* gpu)
		: UserDevice {}, fb {fb}, gpu {gpu}, supports_page_flip {gpu->supports_page_flipping} {
		name = "gpu_fb";
		exclusive = true;
		if (gpu->supports_page_flipping) {
			front_surface = gpu->create_surface();
		}
		back_surface = gpu->create_surface();
	}

	~GpuFbDev() override {
		if (front_surface) {
			gpu->destroy_surface(front_surface);
		}
		gpu->destroy_surface(back_surface);
	}

	int handle_request(const kstd::vector<u8>& data, kstd::vector<u8>& res, usize max_size) override {
		if (data.size() < sizeof(FbLink) || max_size < sizeof(FbLinkResponse)) {
			return ERR_INVALID_ARGUMENT;
		}
		auto* link = reinterpret_cast<const FbLink*>(data.data());
		res.resize(sizeof(FbLinkResponse));
		auto* resp = reinterpret_cast<FbLinkResponse*>(res.data());

		switch (link->op) {
			case FbLinkOpGetInfo:
				resp->info.pitch = fb->pitch;
				resp->info.width = fb->width;
				resp->info.height = fb->height;
				resp->info.bpp = fb->bpp;
				resp->info.flags = gpu->supports_page_flipping ? FB_LINK_DOUBLE_BUFFER : 0;
				break;
			case FbLinkOpMap:
			{
				auto process = get_current_thread()->process;
				usize size = fb->height * fb->pitch;
				auto mem = process->allocate(nullptr, size, PageFlags::Read | PageFlags::Write, MemoryAllocFlags::None, nullptr);
				if (size && !mem) {
					return ERR_NO_MEM;
				}

				auto phys = back_surface->get_phys();

				for (usize i = 0; i < size; i += PAGE_SIZE) {
					if (!process->page_map.map(
							mem + i,
							phys + i,
							PageFlags::Read | PageFlags::Write | PageFlags::User,
							CacheMode::WriteCombine)) {
						for (usize j = 0; j < i; j += PAGE_SIZE) {
							process->page_map.unmap(mem + i);
						}
						process->free(mem, size);
						return ERR_NO_MEM;
					}
				}

				resp->map.mapping = reinterpret_cast<void*>(mem);

				if (fb_log_registered) {
					IrqGuard irq_guard {};
					LOG.lock()->unregister_sink(fb);
					fb_log_registered = false;
				}

				break;
			}
			case FbLinkOpFlip:
			{
				if (supports_page_flip) {
					auto tmp = front_surface;
					front_surface = back_surface;
					back_surface = tmp;
					gpu->flip(front_surface);
				}
				break;
			}
		}

		return 0;
	}

private:
	Framebuffer* fb;
	GpuSurface* front_surface;
	GpuSurface* back_surface;
	Gpu* gpu;
	bool fb_log_registered {true};
	bool supports_page_flip {};
};

struct DummySurface : public GpuSurface {
	explicit DummySurface(usize phys) : phys {phys} {}

	usize get_phys() override {
		return phys;
	}

	usize phys;
};

struct DummyGpu : public Gpu {
	explicit DummyGpu(usize phys) : phys {phys} {}

	GpuSurface* create_surface() override {
		return new DummySurface {phys};
	}

	void destroy_surface(GpuSurface* surface) override {
		delete surface;
	}

	void flip(GpuSurface*) override {}

	usize phys;
};

ManuallyInit<kstd::shared_ptr<GpuFbDev>> BOOT_FB_DEV;
static ManuallyInit<DummyGpu> DUMMY_GPU;

void fb_dev_register_boot_fb() {
	if (!BOOT_FB.is_initialized()) {
		return;
	}

	Gpu* gpu = nullptr;
	IrqGuard irq_guard {};
	auto guard = USER_DEVICES[static_cast<int>(CrescentDeviceTypeGpu)]->lock();
	for (auto& device : *guard) {
		if (static_cast<GpuDevice*>(device.data())->gpu->owns_boot_fb) {
			gpu = static_cast<GpuDevice*>(device.data())->gpu;
			break;
		}
	}

	if (!gpu) {
		println("[kernel][fb]: using dummy gpu with no page flip support");
		DUMMY_GPU.initialize(BOOT_FB->phys);
		gpu = &*DUMMY_GPU;
	}

	BOOT_FB_DEV.initialize(kstd::make_shared<GpuFbDev>(&*BOOT_FB, gpu));
	user_dev_add(*BOOT_FB_DEV, CrescentDeviceTypeFb);
}
