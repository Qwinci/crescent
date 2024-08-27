#pragma once
#include "packet.hpp"
#include "mem/unique_phys.hpp"
#include "expected.hpp"
#include "desc.hpp"
#include "vector.hpp"
#include "span.hpp"

namespace usb {
	struct Device {
		virtual ~Device() = default;

		virtual Status control(setup::Packet setup, usize buffer) = 0;
		virtual bool normal_one(normal::Packet* packet) = 0;
		virtual usize normal_multiple(normal::Packet* packets, usize count) = 0;

		virtual Status set_config(const UniquePhysical& config) = 0;

		void attach();

		kstd::expected<DeviceDescriptor, Status> get_device_descriptor();
		kstd::expected<UniquePhysical, Status> get_config_descriptor(u8 index);
	};

	struct Descriptor {
		u8 type;
		u8 length;
		void* data;
	};

	struct AssignedDevice {
		Device& device;
		u8 interface_index;
		kstd::vector<EndpointDescriptor> eps;
		kstd::vector<Descriptor> descriptors;
	};
}
