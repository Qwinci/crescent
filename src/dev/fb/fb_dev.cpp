#include "dev/dev.hpp"
#include "fb.hpp"
#include "sched/sched.hpp"
#include "sched/process.hpp"

struct FbDev : public Device {
	explicit FbDev(Framebuffer* fb) : Device {}, fb {fb} {
		name = "";
		name += "a";
	}

	int handle_request(const kstd::vector<u8>& data, kstd::vector<u8>& res, usize max_size) override {
		if (data.size() < sizeof(FbLink) || max_size < sizeof(FbLinkResponse)) {
			return ERR_INVALID_ARGUMENT;
		}
		auto* link = reinterpret_cast<const FbLink*>(data.data());
		res.resize(sizeof(FbLinkResponse));
		auto* resp = reinterpret_cast<FbLinkResponse*>(res.data());

		switch (link->op) {
			case FbLinkOp::GetInfo:
				resp->info.pitch = fb->pitch;
				resp->info.width = fb->width;
				resp->info.height = fb->height;
				resp->info.bpp = fb->bpp;
				break;
			case FbLinkOp::Map:
			{
				auto process = get_current_thread()->process;
				usize size = fb->height * fb->pitch;
				auto mem = process->allocate(nullptr, size, MemoryAllocFlags::Read | MemoryAllocFlags::Write, nullptr);
				if (size && !mem) {
					return ERR_NO_MEM;
				}

				for (usize i = 0; i < size; i += PAGE_SIZE) {
					if (!process->page_map.map(
							mem + i,
							fb->phys + i,
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

				IrqGuard irq_guard {};
				LOG.lock()->unregister_sink(fb);
				fb->clear();

				break;
			}
		}

		return 0;
	}

private:
	Framebuffer* fb;
};

ManuallyInit<kstd::shared_ptr<FbDev>> BOOT_FB_DEV;

void fb_dev_register_boot_fb() {
	BOOT_FB_DEV.initialize(kstd::make_shared<FbDev>(&*BOOT_FB));
	dev_add(*BOOT_FB_DEV, CrescentDeviceType::Fb);
}
