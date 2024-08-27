#pragma once
#include "types.hpp"
#include "mem/unique_phys.hpp"
#include "mem/mem.hpp"

namespace usb {
	struct DeviceDescriptor {
		u8 length;
		u8 descriptor_type;
		u16 bcd_usb;
		u8 device_class;
		u8 device_sublass;
		u8 device_protocol;
		u8 max_packet_size0;
		u16 id_vendor;
		u16 id_product;
		u16 bcd_device;
		u8 index_manufacturer;
		u8 index_product;
		u8 index_serial_number;
		u8 num_configurations;
	};

	struct [[gnu::packed]] ConfigDescriptor {
		u8 length;
		u8 descriptor_type;
		u16 total_length;
		u8 num_interfaces;
		u8 config_value;
		u8 config_index;
		u8 attribs;
		u8 max_power;
	};

	struct InterfaceDescriptor {
		u8 length;
		u8 descriptor_type;
		u8 interface_number;
		u8 alternate_setting;
		u8 num_endpoints;
		u8 interface_class;
		u8 interface_subclass;
		u8 interface_protocol;
		u8 interface_index;
	};

	enum class TransferType : u8 {
		Control = 0,
		Isoch = 1,
		Bulk = 0b10,
		Interrupt = 0b11
	};

	enum class IrqUsage : u8 {
		Periodic = 0,
		Notification = 1,
		Reserved0 = 0b10,
		Reserved1 = 0b11
	};

	enum class IsochSync : u8 {
		NoSync = 0,
		Async = 1,
		Adaptive = 0b10,
		Sync = 0b11
	};

	enum class IsochUsage : u8 {
		Data = 0,
		Feedback = 1,
		ImplicitFeedback = 0b10,
		Reserved = 0b11
	};

	struct [[gnu::packed]] EndpointDescriptor {
		u8 length;
		u8 descriptor_type;
		u8 endpoint_addr;
		u8 attribs;
		u16 max_packet_size;
		u8 interval;

		[[nodiscard]] constexpr u8 number() const {
			return (endpoint_addr & 0xF) - 1;
		}

		[[nodiscard]] constexpr bool dir_in() const {
			return endpoint_addr >> 7;
		}

		[[nodiscard]] constexpr TransferType transfer_type() const {
			return static_cast<TransferType>(attribs & 0b11);
		}

		[[nodiscard]] constexpr IrqUsage irq_usage() const {
			return static_cast<IrqUsage>(attribs >> 4 & 0b11);
		}

		[[nodiscard]] constexpr IsochSync isoch_sync() const {
			return static_cast<IsochSync>(attribs >> 2 & 0b11);
		}

		[[nodiscard]] constexpr IsochUsage isoch_usage() const {
			return static_cast<IsochUsage>(attribs >> 4 & 0b11);
		}
	};

	struct SsEndpointCompanionDesc {
		u8 length;
		u8 descriptor_type;
		u8 max_burst;
		u8 attribs;
		u16 bytes_per_interval;
	};

	struct ConfigTraverseData {
		u32 interface_index;
		u32 endpoint_index;
	};

	template<typename F>
	void traverse_config(const UniquePhysical& config, F fn) {
		auto* config_ptr = to_virt<ConfigDescriptor>(config.base);

		ConfigTraverseData data {};

		fn(config_ptr->descriptor_type, config_ptr, config_ptr->length, data);

		usize offset = config_ptr->length;
		for (u32 interface_i = 0; interface_i < config_ptr->num_interfaces; ++interface_i) {
			data.endpoint_index = 0;

			auto* interface = offset(config_ptr, InterfaceDescriptor*, offset);

			while (interface->descriptor_type != usb::setup::desc_type::INTERFACE) {
				fn(interface->descriptor_type, interface, interface->length, data);

				offset += interface->length;
				interface = offset(config_ptr, InterfaceDescriptor*, offset);
			}

			fn(interface->descriptor_type, interface, interface->length, data);
			offset += interface->length;

			for (u32 ep_i = 0; ep_i < interface->num_endpoints; ++ep_i) {
				auto* ep = offset(config_ptr, EndpointDescriptor*, offset);

				while (ep->descriptor_type != usb::setup::desc_type::ENDPOINT) {
					fn(ep->descriptor_type, ep, ep->length, data);

					offset += ep->length;
					ep = offset(config_ptr, EndpointDescriptor*, offset);
				}

				fn(ep->descriptor_type, ep, ep->length, data);

				offset += ep->length;
				++data.endpoint_index;
			}

			++data.interface_index;
		}
	}
}
