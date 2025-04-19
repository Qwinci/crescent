#include "arch/irq.hpp"
#include "arch/x86/mod.hpp"
#include "dev/clock.hpp"
#include "dev/pci.hpp"
#include "dev/sound/sound_dev.hpp"
#include "mem/pmalloc.hpp"
#include "mem/vspace.hpp"
#include "sched/process.hpp"
#include "utils/driver.hpp"

#include <uhda/kernel_api.h>
#include <uhda/uhda.h>

UhdaStatus uhda_kernel_pci_read(void* pci_device, uint8_t offset, uint8_t size, uint32_t* res) {
	auto* device = static_cast<pci::Device*>(pci_device);
	switch (size) {
		case 1:
			*res = device->read8(offset);
			break;
		case 2:
			*res = device->read16(offset);
			break;
		case 4:
			*res = device->read32(offset);
			break;
		default:
			return UHDA_STATUS_UNSUPPORTED;
	}

	return UHDA_STATUS_SUCCESS;
}

UhdaStatus uhda_kernel_pci_write(void* pci_device, uint8_t offset, uint8_t size, uint32_t value) {
	auto* device = static_cast<pci::Device*>(pci_device);
	switch (size) {
		case 1:
			device->write8(offset, value);
			break;
		case 2:
			device->write16(offset, value);
			break;
		case 4:
			device->write32(offset, value);
			break;
		default:
			return UHDA_STATUS_UNSUPPORTED;
	}

	return UHDA_STATUS_SUCCESS;
}

struct HdaIrqHandler {
	IrqHandler handler;
	u8 vec;
};

UhdaStatus uhda_kernel_pci_allocate_irq(void* pci_device, UhdaIrqHint hint, UhdaIrqHandlerFn fn, void* arg, void** opaque_irq) {
	auto* device = static_cast<pci::Device*>(pci_device);

	auto pci_flags = hint == UHDA_IRQ_HINT_ANY ? pci::IrqFlags::All : pci::IrqFlags::Legacy;

	auto irq_count = device->alloc_irqs(1, 1, pci_flags | pci::IrqFlags::Shared);
	assert(irq_count);
	auto irq = device->get_irq(0);

	auto irq_handler = new HdaIrqHandler {
		.handler {
			.fn = [=](IrqFrame*) {
				return fn(arg);
			},
			.can_be_shared = true
		},
		.vec = static_cast<u8>(irq)
	};

	register_irq_handler(irq, &irq_handler->handler);

	*opaque_irq = irq_handler;

	return UHDA_STATUS_SUCCESS;
}

void uhda_kernel_pci_deallocate_irq(void* pci_device, void* opaque_irq) {
	auto* device = static_cast<pci::Device*>(pci_device);

	device->free_irqs();

	auto* irq = static_cast<HdaIrqHandler*>(opaque_irq);
	deregister_irq_handler(irq->vec, &irq->handler);
	delete irq;
}

void uhda_kernel_pci_enable_irq(void* pci_device, void*, bool enable) {
	auto* device = static_cast<pci::Device*>(pci_device);
	device->enable_irqs(enable);
}

UhdaStatus uhda_kernel_pci_map_bar(void* pci_device, uint32_t bar, void** virt) {
	auto* device = static_cast<pci::Device*>(pci_device);

	auto mapping = device->map_bar(bar);
	assert(mapping);
	*virt = mapping;

	return UHDA_STATUS_SUCCESS;
}

void uhda_kernel_pci_unmap_bar(void*, uint32_t, void*) {}

void* uhda_kernel_malloc(size_t size) {
	return ALLOCATOR.alloc(size);
}

void uhda_kernel_free(void* ptr, size_t size) {
	ALLOCATOR.free(ptr, size);
}

void uhda_kernel_delay(uint32_t microseconds) {
	udelay(microseconds);
}

void uhda_kernel_log(const char* str) {
	println("uHDA: ", str);
}

UhdaStatus uhda_kernel_allocate_physical(size_t size, uintptr_t* res) {
	*res = pmalloc((size + 0xFFF) / 0x1000);
	if (!*res) {
		return UHDA_STATUS_NO_MEMORY;
	}
	return UHDA_STATUS_SUCCESS;
}

void uhda_kernel_deallocate_physical(uintptr_t phys, size_t size) {
	pfree(phys, (size + 0xFFF) / 0x1000);
}

UhdaStatus uhda_kernel_map(uintptr_t phys, size_t size, void** virt) {
	auto* ptr = KERNEL_VSPACE.alloc(size);
	assert(ptr);

	auto& KERNEL_MAP = KERNEL_PROCESS->page_map;

	for (usize i = 0; i < size; i += PAGE_SIZE) {
		auto status = KERNEL_MAP.map(
			reinterpret_cast<u64>(ptr) + i,
			phys + i,
			PageFlags::Read | PageFlags::Write,
			CacheMode::Uncached);
		assert(status);
	}

	*virt = ptr;

	return UHDA_STATUS_SUCCESS;
}

void uhda_kernel_unmap(void* virt, size_t size) {
	auto& KERNEL_MAP = KERNEL_PROCESS->page_map;

	for (usize i = 0; i < size; i += PAGE_SIZE) {
		KERNEL_MAP.unmap(reinterpret_cast<u64>(virt) + i);
	}

	KERNEL_VSPACE.free(virt, size);
}

UhdaStatus uhda_kernel_create_spinlock(void** spinlock) {
	*spinlock = new Spinlock<void>();
	return UHDA_STATUS_SUCCESS;
}

void uhda_kernel_free_spinlock(void* spinlock) {
	delete static_cast<Spinlock<void>*>(spinlock);
}

UhdaIrqState uhda_kernel_lock_spinlock(void* spinlock) {
	auto* lock = static_cast<Spinlock<void>*>(spinlock);

	auto state = arch_enable_irqs(false);
	lock->manual_lock();
	return state;
}

void uhda_kernel_unlock_spinlock(void* spinlock, UhdaIrqState irq_state) {
	auto* lock = static_cast<Spinlock<void>*>(spinlock);

	lock->manual_unlock();
	arch_enable_irqs(irq_state);
}

struct HdaCodecDev : SoundDevice {
	HdaCodecDev(UhdaController* ctrl, const UhdaCodec* codec) {
		uhda_codec_get_output_groups(codec, &output_groups, &output_group_count);

		UhdaStream** all_streams;
		size_t stream_count;
		uhda_get_output_streams(ctrl, &all_streams, &stream_count);

		assert(stream_count);
		stream = all_streams[0];
	}

	int get_info(SoundDeviceInfo& res) override {
		res.output_count = output_group_count;
		return 0;
	}

	int get_output_info(usize index, SoundOutputInfo& res) override {
		if (index >= output_group_count) {
			return ERR_INVALID_ARGUMENT;
		}

		auto* group = output_groups[index];

		const UhdaOutput* const* outputs;
		size_t output_count;
		uhda_output_group_get_outputs(group, &outputs, &output_count);
		assert(output_count);

		auto info = uhda_output_get_info(outputs[0]);

		kstd::string_view type_str {};

		switch (info.type) {
			case UHDA_OUTPUT_TYPE_LINE_OUT:
				res.type = SoundDeviceTypeLineOut;
				type_str = "line out";
				break;
			case UHDA_OUTPUT_TYPE_SPEAKER:
				res.type = SoundDeviceTypeSpeaker;
				type_str = "speaker";
				break;
			case UHDA_OUTPUT_TYPE_HEADPHONE:
				res.type = SoundDeviceTypeHeadphone;
				type_str = "headphone";
				break;
			default:
				res.type = SoundDeviceTypeUnknown;
				type_str = "unknown";
				break;
		}

		auto loc_str = UHDA_LOCATION_STRINGS[info.location];
		auto loc_len = strlen(loc_str);

		memcpy(res.name, type_str.data(), type_str.size());
		memcpy(res.name + type_str.size(), " at ", 4);
		memcpy(res.name + type_str.size() + 4, loc_str, loc_len + 1);
		res.name_len = type_str.size() + 4 + loc_len;

		res.buffer_size = 0x1000 * 4;
		res.id = reinterpret_cast<void*>(index);

		return 0;
	}

	int set_active_output(void* id) override {
		auto index = reinterpret_cast<size_t>(id);
		if (index >= output_group_count) {
			return ERR_INVALID_ARGUMENT;
		}

		if (active_path) {
			auto status = uhda_path_shutdown(active_path);
			assert(status == UHDA_STATUS_SUCCESS);
		}

		auto* group = output_groups[index];

		const UhdaOutput* const* outputs;
		size_t output_count;
		uhda_output_group_get_outputs(group, &outputs, &output_count);
		assert(output_count);

		auto status = uhda_find_path(outputs[0], nullptr, 0, false, &active_path);
		assert(status == UHDA_STATUS_SUCCESS);
		return 0;
	}

	int set_output_params(SoundOutputParams& params) override {
		if (!active_path) {
			return ERR_UNSUPPORTED;
		}

		UhdaFormat fmt {UHDA_FORMAT_PCM8};
		switch (params.fmt) {
			case SoundFormatNone:
			case SoundFormatPcmU8:
				break;
			case SoundFormatPcmU16:
				fmt = UHDA_FORMAT_PCM16;
				break;
			case SoundFormatPcmU20:
				fmt = UHDA_FORMAT_PCM20;
				break;
			case SoundFormatPcmU24:
				fmt = UHDA_FORMAT_PCM24;
				break;
			case SoundFormatPcmU32:
				fmt = UHDA_FORMAT_PCM32;
				break;
		}

		UhdaStreamParams uhda_params {
			.sample_rate = params.sample_rate,
			.channels = params.sample_rate,
			.fmt = fmt
		};

		auto status = uhda_stream_setup(stream, &uhda_params, 1024 * 1024, nullptr, nullptr, 0, nullptr, nullptr);
		assert(status == UHDA_STATUS_SUCCESS);

		status = uhda_path_setup(active_path, &uhda_params, stream);
		assert(status == UHDA_STATUS_SUCCESS);

		SoundFormat actual_fmt;
		switch (uhda_params.fmt) {
			case UHDA_FORMAT_PCM8:
				actual_fmt = SoundFormatPcmU8;
				break;
			case UHDA_FORMAT_PCM16:
				actual_fmt = SoundFormatPcmU16;
				break;
			case UHDA_FORMAT_PCM20:
				actual_fmt = SoundFormatPcmU20;
				break;
			case UHDA_FORMAT_PCM24:
				actual_fmt = SoundFormatPcmU24;
				break;
			case UHDA_FORMAT_PCM32:
				actual_fmt = SoundFormatPcmU32;
				break;
		}

		params.sample_rate = uhda_params.sample_rate;
		params.channels = uhda_params.channels;
		params.fmt = actual_fmt;

		return 0;
	}

	int set_volume(u8 percentage) override {
		if (!active_path) {
			return UHDA_STATUS_UNSUPPORTED;
		}

		auto status = uhda_path_set_volume(active_path, percentage);
		assert(status == UHDA_STATUS_SUCCESS);

		return 0;
	}

	int queue_output(const void* buffer, usize& size) override {
		u32 small_size = size;

		auto status = uhda_stream_queue_data(stream, buffer, &small_size);
		assert(status == UHDA_STATUS_SUCCESS);

		size = small_size;

		return 0;
	}

	int play(bool play) override {
		if (!active_path) {
			return UHDA_STATUS_UNSUPPORTED;
		}

		auto status = uhda_stream_play(stream, play);
		assert(status == UHDA_STATUS_SUCCESS);

		return 0;
	}

	int wait_until_consumed(usize trip_size, usize& remaining) override {
		println("[kernel][hda]: wait_until_consumed is not supported");
		return 0;
	}

	const UhdaOutputGroup* const* output_groups {};
	size_t output_group_count {};
	UhdaPath* active_path {};
	UhdaStream* stream {};
};

static InitStatus hda_init(pci::Device& device) {
	println("[kernel][hda]: init");

	UhdaController* ctrl;
	auto status = uhda_init(&device, &ctrl);
	assert(status == UHDA_STATUS_SUCCESS);

	const UhdaCodec* const* codecs;
	size_t codec_count;
	uhda_get_codecs(ctrl, &codecs, &codec_count);

	assert(codec_count);

	for (size_t i = 0; i < codec_count; ++i) {
		auto dev = kstd::make_shared<HdaCodecDev>(ctrl, codecs[i]);
		user_dev_add(std::move(dev), CrescentDeviceTypeSound);
	}

	/*Module sound_module {};
	auto module_status = x86_get_module(sound_module, "output.raw");
	assert(module_status);

	UhdaStream** streams;
	size_t stream_count;
	uhda_get_output_streams(ctrl, &streams, &stream_count);
	assert(stream_count);

	const UhdaOutputGroup* const* output_groups;
	size_t output_group_count;
	uhda_codec_get_output_groups(codecs[0], &output_groups, &output_group_count);
	assert(output_group_count);

	const UhdaOutput* chosen_output = nullptr;

	for (size_t i = 0; i < output_group_count; ++i) {
		const UhdaOutput* const* outputs;
		size_t output_count;
		uhda_output_group_get_outputs(output_groups[i], &outputs, &output_count);

		assert(output_count);

		size_t index = 0;

		for (size_t j = 0; j < output_count; ++j) {
			auto output_info = uhda_output_get_info(outputs[j]);
			println("candidate output type ", output_info.type);

			if (output_info.type != UHDA_OUTPUT_TYPE_LINE_OUT &&
				output_info.type != UHDA_OUTPUT_TYPE_HEADPHONE) {
				continue;
			}

			bool presence;
			status = uhda_output_get_presence(outputs[j], &presence);
			if (status == UHDA_STATUS_UNSUPPORTED) {
				println("presence detect not supported, using ", j);
				index = j;
				break;
			}
			else {
				assert(status == UHDA_STATUS_SUCCESS);
				if (presence) {
					println("present");
					index = j;
					break;
				}
				else {
					println("not present");
				}
			}
		}

		chosen_output = outputs[index];

		auto output_info = uhda_output_get_info(outputs[index]);
		println("chosen output type ", output_info.type);
		break;
	}

	if (!chosen_output) {
		println("no output, return");
		return InitStatus::Success;
	}

	UhdaPath* path;
	status = uhda_find_path(chosen_output, nullptr, 0, false, &path);
	assert(status == UHDA_STATUS_SUCCESS);

	UhdaStreamParams params {
		.sample_rate = 44100,
		.channels = 2,
		.fmt = UHDA_FORMAT_PCM16
	};

	status = uhda_stream_setup(streams[0], &params, sound_module.size, nullptr, nullptr);
	assert(status == UHDA_STATUS_SUCCESS);

	uint32_t queued_size = sound_module.size;
	status = uhda_stream_queue_data(streams[0], sound_module.data, &queued_size);
	assert(status == UHDA_STATUS_SUCCESS);

	struct State {
		Event event;
	};

	auto state = new State {};

	for (int j = 0;; ++j) {
		status = uhda_path_setup(path, &params, streams[0]);
		assert(status == UHDA_STATUS_SUCCESS);

		status = uhda_path_set_volume(path, 40);
		assert(status == UHDA_STATUS_SUCCESS);

		status = uhda_stream_setup(
			streams[0],
			&params,
			17600,
			nullptr,
			nullptr,
			8800,
			[](void* arg, uint32_t remaining) {
				auto* state = static_cast<State*>(arg);
				state->event.signal_one_if_not_pending();
			},
			state);

		auto stream_status = uhda_stream_get_status(streams[0]);

		for (size_t i = 0; i < sound_module.size;) {
			uint32_t space;
			uhda_stream_get_remaining(streams[0], &space);

			if (space == 17600) {
				state->event.wait();
				continue;
			}

			uint32_t to_copy = 17600 - space;
			if (to_copy > sound_module.size - i) {
				to_copy = sound_module.size - i;
			}
			to_copy = kstd::min(to_copy, 0x1000U);

			uhda_stream_queue_data(streams[0], offset(sound_module.data, void*, i), &to_copy);

			i += to_copy;

			space += to_copy;
			if (stream_status == UHDA_STREAM_STATUS_PAUSED &&
				space >= 8800) {
				println("hda: starting stream");

				uhda_stream_play(streams[0], true);
				stream_status = UHDA_STREAM_STATUS_RUNNING;
			}
		}

		while (true) {
			uint32_t remaining;
			uhda_stream_get_remaining(streams[0], &remaining);
			if (!remaining) {
				break;
			}

			state->event.wait();
		}

		uhda_stream_shutdown(streams[0]);
		uhda_path_shutdown(path);

		println("restart");
	}*/

	return InitStatus::Success;
}

static constexpr PciDriver HDA_CLASS_DRIVER {
	.init = hda_init,
	.match = PciMatch::Class | PciMatch::Subclass,
	._class = UHDA_MATCHING_CLASS,
	.subclass = UHDA_MATCHING_SUBCLASS
};

static constexpr PciDriver HDA_DEVICE_DRIVER {
	.init = hda_init,
	.match = PciMatch::Device,
	.devices {
		UHDA_MATCHING_DEVICES
	}
};

PCI_DRIVER(HDA_CLASS_DRIVER);
PCI_DRIVER(HDA_DEVICE_DRIVER);
