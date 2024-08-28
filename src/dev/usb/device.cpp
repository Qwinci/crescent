#include "device.hpp"
#include "arch/paging.hpp"
#include "desc.hpp"
#include "mem/mem.hpp"
#include "sched/sched.hpp"
#include "sched/process.hpp"
#include "arch/cpu.hpp"
#include "utils/driver.hpp"

using namespace usb;

extern Driver DRIVERS_START[];
extern Driver DRIVERS_END[];

static void device_attach_fn(void* arg) {
	do {
		auto* device = static_cast<usb::Device*>(arg);

		auto dev_desc = device->get_device_descriptor();
		if (!dev_desc) {
			println("[kernel][usb]: failed to get device descriptor, status: ", static_cast<u8>(dev_desc.error()));
			break;
		}

		auto config = device->get_config_descriptor(0);
		if (!config) {
			println("[kernel][usb]: failed to get config descriptor, status: ", static_cast<u8>(config.error()));
			break;
		}

		kstd::vector<Interface> interfaces {};
		kstd::vector<Descriptor> descriptors {};
		traverse_config(config.value(), [&](u8 type, void* data, u8 length, ConfigTraverseData& config_data) {
			if (type == setup::desc_type::INTERFACE) {
				interfaces.push({
					.desc {*static_cast<InterfaceDescriptor*>(data)},
					.eps {},
					.descs {}
				});
			}
			else if (type == setup::desc_type::ENDPOINT) {
				assert(!interfaces.is_empty());
				interfaces.back()->eps.push(*static_cast<EndpointDescriptor*>(data));
			}
			else if (!interfaces.is_empty()) {
				interfaces.back()->descs.push({
					.type = type,
					.length = length,
					.data = data
				});
			}
			else {
				descriptors.push({
					.type = type,
					.length = length,
					.data = data
				});
			}
		});

		auto status = device->set_config(config.value());
		if (status != usb::Status::Success) {
			println("[kernel][usb]: failed to set config, status: ", static_cast<u8>(status));
			break;
		}

		AssignedDevice assigned_device {
			.device = *device,
			.interfaces {std::move(interfaces)},
			.descs {std::move(descriptors)},
			.data = nullptr,
			.interface_index = 0
		};

		Config parsed_config {
			.raw = config.value(),
			.interfaces {assigned_device.interfaces}
		};

		for (usize i = 0; i < parsed_config.interfaces.size(); ++i) {
			auto& interface = parsed_config.interfaces[i];

			assigned_device.interface_index = i;

			auto assoc = assigned_device.get_iface_assoc();

			for (auto* driver = DRIVERS_START; driver != DRIVERS_END; ++driver) {
				if (driver->type != DriverType::Usb) {
					continue;
				}

				auto& usb_driver = driver->usb;

				if (driver->usb->match & UsbMatch::InterfaceClass) {
					if (interface.desc.interface_class != usb_driver->interface_class) {
						continue;
					}
				}

				if (driver->usb->fine_match) {
					if (auto data = driver->usb->fine_match(parsed_config, dev_desc.value())) {
						assigned_device.data = data;
					}
					else {
						continue;
					}
				}

				if (driver->usb->init(assigned_device)) {
					if (assoc) {
						i = assoc->first_interface + assoc->interface_count;
					}
					break;
				}

				assigned_device.data = nullptr;
			}
		}
	} while (false);

	IrqGuard irq_guard {};
	auto* cpu = get_current_thread()->cpu;
	cpu->scheduler.exit_thread(0);
}

Status usb::Device::control(setup::Packet setup) {
	control_async(&setup);
	setup.event.wait();
	return setup.event.status;
}

void usb::Device::attach() {
	IrqGuard irq_guard {};
	auto cpu = get_current_thread()->cpu;
	auto thread = new Thread {"usb device attach handler", cpu, &*KERNEL_PROCESS, device_attach_fn, this};
	thread->pin_cpu = true;
	thread->pin_level = true;
	cpu->scheduler.queue(thread);
}

kstd::expected<DeviceDescriptor, Status> usb::Device::get_device_descriptor() {
	auto desc_phys = UniquePhysical::create(1);
	auto status = control({
		Dir::FromDevice,
		setup::Type::Standard,
		setup::Rec::Device,
		setup::std_req::GET_DESCRIPTOR,
		setup::desc_type::DEVICE << 8,
		0,
		desc_phys->base,
		sizeof(DeviceDescriptor)
	});
	if (status != Status::Success) {
		return status;
	}

	auto desc = *to_virt<DeviceDescriptor>(desc_phys->base);
	return desc;
}

kstd::expected<UniquePhysical, Status> usb::Device::get_config_descriptor(u8 index) {
	auto config = UniquePhysical::create(1);

	auto status = control({
		Dir::FromDevice,
		setup::Type::Standard,
		setup::Rec::Device,
		setup::std_req::GET_DESCRIPTOR,
		static_cast<u16>(setup::desc_type::CONFIGURATION << 8 | index),
		0,
		config->base,
		8
	});
	if (status != Status::Success) {
		return status;
	}

	auto desc = to_virt<ConfigDescriptor>(config->base);
	auto size = desc->total_length;

	if (size > PAGE_SIZE) {
		config = UniquePhysical::create(ALIGNUP(size, PAGE_SIZE) / PAGE_SIZE);
	}

	status = control({
		Dir::FromDevice,
		setup::Type::Standard,
		setup::Rec::Device,
		setup::std_req::GET_DESCRIPTOR,
		setup::desc_type::CONFIGURATION << 8,
		0,
		config->base,
		size
	});
	if (status != Status::Success) {
		return status;
	}

	return std::move(config).value();
}

InterfaceAssocDescriptor* AssignedDevice::get_iface_assoc() const {
	for (auto& desc : descs) {
		if (desc.type == setup::desc_type::INTERFACE_ASSOCIATION) {
			auto* assoc = static_cast<InterfaceAssocDescriptor*>(desc.data);
			if (interface_index >= assoc->first_interface &&
				interface_index < assoc->first_interface + assoc->interface_count) {
				return assoc;
			}
		}
	}

	return nullptr;
}
