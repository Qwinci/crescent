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

		Status control(setup::Packet setup);

		virtual void control_async(setup::Packet* setup) = 0;
		virtual bool normal_one(normal::Packet* packet) = 0;
		virtual usize normal_multiple(normal::Packet* packets, usize count) = 0;
		virtual usize normal_large(normal::LargePacket* packet) = 0;

		virtual Status set_config(const UniquePhysical& config) = 0;

		void attach();

		kstd::expected<DeviceDescriptor, Status> get_device_descriptor();
		kstd::expected<UniquePhysical, Status> get_config_descriptor(u8 index);

		usize max_normal_packets {};
	};

	struct Descriptor {
		u8 type;
		u8 length;
		void* data;
	};

	struct Interface {
		InterfaceDescriptor desc {};
		kstd::vector<EndpointDescriptor> eps;
		kstd::vector<Descriptor> descs;
	};

	struct Config {
		const UniquePhysical& raw;
		const kstd::vector<Interface>& interfaces;
	};

	struct AssignedDevice {
		Device& device;
		kstd::vector<Interface> interfaces;
		kstd::vector<Descriptor> descs;
		void* data;
		u8 interface_index;

		[[nodiscard]] InterfaceAssocDescriptor* get_iface_assoc() const;
	};
}
