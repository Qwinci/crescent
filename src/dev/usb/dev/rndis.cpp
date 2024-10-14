#include "dev/net/dhcp.hpp"
#include "dev/net/ethernet.hpp"
#include "dev/net/nic/nic.hpp"
#include "mem/vspace.hpp"
#include "sched/process.hpp"
#include "utils/driver.hpp"

namespace {
	struct Info {};

	Info NORMAL {};
	Info POLL_STATUS_BEFORE_CONTROL {};

	inline bool interface_matches(const usb::InterfaceDescriptor& desc, u8 _class, u8 subclass, u8 protocol) {
		return desc.interface_class == _class && desc.interface_subclass == subclass && desc.interface_protocol == protocol;
	}

	Info* get_comm_interface(const usb::Interface& interface) {
		if (interface_matches(interface.desc, 2, 2, 0xFF) ||
			interface_matches(interface.desc, 0xEF, 4, 1)) {
			return &NORMAL;
		}
		else if (interface_matches(interface.desc, 0xEF, 1, 1)) {
			return &POLL_STATUS_BEFORE_CONTROL;
		}
		else {
			return nullptr;
		}
	}

	bool is_data_interface(const usb::Interface& interface) {
		return interface_matches(interface.desc, 0xA, 0, 0);
	}

	constexpr u32 STATUS_SUCCESS = 0;

	struct RndisInitializeMsg {
		u32 msg_type {2};
		u32 msg_length {24};
		u32 request_id {};
		u32 major_version {1};
		u32 minor_version {};
		u32 max_transfer_size {0x1000};
	};

	struct RndisInitializeCmplt {
		u32 msg_type;
		u32 msg_length;
		u32 request_id;
		u32 status;
		u32 major_version;
		u32 minor_version;
		u32 device_flags;
		u32 medium;
		u32 max_packets_per_transfer;
		u32 max_transfer_size;
		u32 packet_align_factor;
		u32 reserved[2];
	};

	struct RndisQueryMsg {
		u32 msg_type {4};
		u32 msg_len {28};
		u32 request_id {};
		u32 oid {};
		u32 info_buffer_len {};
		u32 info_buffer_offset {};
		u32 reserved {};
	};

	struct RndisQueryCmplt {
		u32 msg_type;
		u32 msg_len;
		u32 request_id;
		u32 status;
		u32 info_buffer_len;
		u32 info_buffer_offset;
	};

	struct RndisSetMsg {
		u32 msg_type {5};
		u32 msg_len {};
		u32 request_id {};
		u32 oid {};
		u32 info_buffer_len {};
		u32 info_buffer_offset {};
		u32 reserved {};
	};

	struct RndisSetCmplt {
		u32 msg_type;
		u32 msg_len;
		u32 request_id;
		u32 status;
	};

	constexpr u32 OID_802_3_PERMANENT_ADDRESS = 16843009;
	constexpr u32 OID_GEN_CURRENT_PACKET_FILTER = 65806;

	constexpr u32 PACKET_TYPE_DIRECTED = 1;
	constexpr u32 PACKET_TYPE_ALL_MULTICAST = 4;
	constexpr u32 PACKET_TYPE_BROADCAST = 8;
	constexpr u32 PACKET_TYPE_PROMISCUOUS = 0x20;

	constexpr u32 DEFAULT_PACKET_FILTER =
		PACKET_TYPE_DIRECTED |
		PACKET_TYPE_ALL_MULTICAST |
		PACKET_TYPE_BROADCAST |
		PACKET_TYPE_PROMISCUOUS;

	struct RndisPacketMsg {
		u32 msg_type {1};
		u32 msg_len {};
		u32 data_offset {};
		u32 data_len {};
		u32 out_of_band_data_offset {};
		u32 out_of_band_data_len {};
		u32 num_out_of_band_data_elems {};
		u32 per_packet_info_offset {};
		u32 per_packet_info_len {};
		u32 reserved[2] {};
	};

	struct RndisState : public Nic {
		RndisState(
			usb::Device& device,
			u8 comm_iface_index,
			u8 comm_in_ep_index,
			u8 data_in_ep_index,
			u8 data_out_ep_index,
			bool poll_notification,
			u16 data_max_packet_size)
			: device {device},
			comm_iface_index {comm_iface_index},
			comm_in_ep_index {comm_in_ep_index},
			data_in_ep_index {data_in_ep_index},
			data_out_ep_index {data_out_ep_index},
			poll_notification {poll_notification},
			data_max_packet_size {data_max_packet_size} {}

		usb::Device& device;
		u8 comm_iface_index;
		u8 comm_in_ep_index;
		u8 data_in_ep_index;
		u8 data_out_ep_index;
		bool poll_notification;
		u16 data_max_packet_size;
		UniquePhysical notification {UniquePhysical::create(1).value()};
		UniquePhysical control_buffer {UniquePhysical::create(1).value()};
		void* control_virt {};
		IrqSpinlock<u32> packet_index {};

		void init() {
			control_virt = KERNEL_VSPACE.alloc(PAGE_SIZE * 4);
			assert(control_virt);
			for (usize i = 0; i < 4 * PAGE_SIZE; i += PAGE_SIZE) {
				assert(KERNEL_PROCESS->page_map.map(
					reinterpret_cast<u64>(control_virt) + i,
					control_buffer.base + i,
					PageFlags::Read | PageFlags::Write,
					CacheMode::Uncached));
			}

			RndisInitializeMsg init_msg {};
			memcpy(control_virt, &init_msg, sizeof(RndisInitializeMsg));
			control(control_buffer.base, sizeof(RndisInitializeMsg));

			RndisQueryMsg query_msg {
				.oid = OID_802_3_PERMANENT_ADDRESS
			};
			memcpy(control_virt, &query_msg, sizeof(query_msg));
			auto recv = control(control_buffer.base, sizeof(query_msg));

			assert(recv >= sizeof(RndisQueryCmplt) + 6);
			auto* hdr = static_cast<RndisQueryCmplt*>(control_virt);
			assert(hdr->status == STATUS_SUCCESS);

			memcpy(mac.data.data, offset(control_virt, void*, 8 + hdr->info_buffer_offset), 6);

			RndisSetMsg set_msg {
				.msg_len = sizeof(RndisSetMsg) + 4,
				.oid = OID_GEN_CURRENT_PACKET_FILTER,
				.info_buffer_len = 4,
				.info_buffer_offset = sizeof(RndisSetMsg) - 8
			};
			memcpy(control_virt, &set_msg, sizeof(set_msg));
			*offset(control_virt, u32*, sizeof(RndisSetMsg)) = DEFAULT_PACKET_FILTER;
			recv = control(control_buffer.base, sizeof(set_msg) + 4);

			assert(recv >= sizeof(RndisSetCmplt));
			assert(hdr->status == STATUS_SUCCESS);

			init_send();
		}

		void run() {
			kstd::vector<usb::normal::Packet> packets;
			kstd::vector<void*> ptrs;
			packets.reserve(128);
			ptrs.reserve(128);
			for (usize i = 0; i < 128; ++i) {
				auto page = pmalloc(1);
				assert(page);
				auto virt = KERNEL_VSPACE.alloc(PAGE_SIZE);
				assert(virt);
				assert(KERNEL_PROCESS->page_map.map(
					reinterpret_cast<u64>(virt),
					page,
					PageFlags::Read | PageFlags::Write,
					CacheMode::Uncached));

				packets.push(usb::normal::Packet {
					usb::Dir::FromDevice,
					data_in_ep_index,
					page,
					PAGE_SIZE
				});
				ptrs.push(virt);
			}

			auto count = device.normal_multiple(packets.data(), device.max_normal_packets);

			while (true) {
				for (usize i = 0; i < count; ++i) {
					auto* packet = &packets[i];
					packet->event.wait();
					if (packet->event.status == usb::Status::Detached) {
						goto end;
					}

					u8* ptr = static_cast<u8*>(ptrs[i]);

					auto total_size = packet->event.len;
					usize offset = 0;

					while (offset < total_size) {
						if (offset + sizeof(RndisPacketMsg) > total_size) {
							println("[kernel][usb]: invalid rndis packet");
							break;
						}

						RndisPacketMsg hdr {};
						memcpy(&hdr, ptr + offset, sizeof(hdr));
						assert(hdr.msg_type == 1);
						auto size = hdr.data_len;

						offset += 8 + hdr.data_offset;

						if (offset + size > total_size) {
							println("[kernel][usb]: invalid rndis packet");
							break;
						}

						ethernet_process_packet(*this, ptr + offset, size);

						offset += size;
					}

					assert(device.normal_one(packet));
				}
			}

			end:
			for (auto ptr : ptrs) {
				KERNEL_PROCESS->page_map.unmap(reinterpret_cast<u64>(ptr));
				KERNEL_VSPACE.free(ptr, PAGE_SIZE);
			}
			for (auto& packet : packets) {
				pfree(packet.buffer, 1);
			}
		}

		kstd::vector<usb::normal::Packet> send_packets;
		kstd::vector<void*> send_ptrs;

		void init_send() {
			send_packets.reserve(128);
			send_ptrs.reserve(128);
			for (usize i = 0; i < 128; ++i) {
				auto page = pmalloc(1);
				assert(page);
				auto virt = KERNEL_VSPACE.alloc(PAGE_SIZE * 4);
				assert(virt);
				assert(KERNEL_PROCESS->page_map.map(
					reinterpret_cast<u64>(virt),
					page,
					PageFlags::Read | PageFlags::Write,
					CacheMode::Uncached));

				send_packets.push(usb::normal::Packet {
					usb::Dir::ToDevice,
					data_out_ep_index,
					page,
					PAGE_SIZE
				});
				send_packets[i].event.signal_one();
				send_ptrs.push(virt);
			}
		}

		void send(const void* data, u32 size) override {
			assert(size + sizeof(RndisPacketMsg) <= PAGE_SIZE);

			u32 index;
			{
				auto guard = packet_index.lock();
				index = *guard;
				++*guard;
				if (*guard == device.max_normal_packets) {
					*guard = 0;
				}
			}

			RndisPacketMsg msg {
				.msg_len = static_cast<u32>(sizeof(RndisPacketMsg) + size),
				.data_offset = sizeof(RndisPacketMsg) - 8,
				.data_len = size
			};

			auto& packet = send_packets[index];
			packet.event.wait();
			assert(!packet.event.is_pending());

			packet.length = msg.msg_len;
			memcpy(send_ptrs[index], &msg, sizeof(msg));
			memcpy(offset(send_ptrs[index], void*, sizeof(RndisPacketMsg)), data, size);

			assert(device.normal_one(&packet));
			packet.event.wait();
			assert(packet.event.status == usb::Status::Success);
			assert(!packet.event.is_pending());
		}

		usize control(usize buffer, u16 length) {
			usb::setup::Packet packet {
				usb::Dir::ToDevice,
				usb::setup::Type::Class,
				usb::setup::Rec::Interface,
				0,
				0,
				comm_iface_index,
				buffer,
				length
			};
			auto status = device.control(packet);
			assert(status == usb::Status::Success);

			if (poll_notification) {
				usb::normal::Packet poll_packet {
					usb::Dir::FromDevice,
					comm_in_ep_index,
					notification.base,
					4
				};
				device.normal_one(&poll_packet);
				poll_packet.event.wait();
				assert(poll_packet.event.status == usb::Status::Success);
			}

			usb::setup::Packet receive_packet {
				usb::Dir::FromDevice,
				usb::setup::Type::Class,
				usb::setup::Rec::Interface,
				1,
				0,
				comm_iface_index,
				buffer,
				PAGE_SIZE
			};
			device.control_async(&receive_packet);
			receive_packet.event.wait();
			assert(receive_packet.event.status == usb::Status::Success || receive_packet.event.status == usb::Status::ShortPacket);
			return receive_packet.event.len;
		}
	};
}

static bool rndis_init(const usb::AssignedDevice& device) {
	println("[kernel][usb]: rndis init");

	auto assoc = device.get_iface_assoc();

	Info* comm_info = nullptr;
	u8 comm_index = 0;
	u8 data_index = 0;

	u32 start;
	u32 count;
	if (assoc) {
		start = assoc->first_interface;
		count = assoc->interface_count;
	}
	else {
		start = device.interface_index;
		count = device.interfaces.size() - start;
	}

	for (u32 i = 0; i < count; ++i) {
		auto& iface = device.interfaces[start + i];
		if (auto value = get_comm_interface(iface)) {
			comm_info = value;
			comm_index = i;
		}
		else if (is_data_interface(iface)) {
			data_index = i;
		}
	}

	u8 comm_in_ep = 0;
	for (auto& ep : device.interfaces[comm_index].eps) {
		if (ep.dir_in()) {
			comm_in_ep = ep.number();
		}
	}

	u8 data_in_ep = 0;
	u8 data_out_ep = 0;
	u16 data_max_packet_size = UINT16_MAX;
	assert(device.interfaces[data_index].eps.size() == 2);
	for (auto& ep : device.interfaces[data_index].eps) {
		if (ep.dir_in()) {
			data_in_ep = ep.number();
		}
		else {
			data_out_ep = ep.number();
		}
		data_max_packet_size = kstd::min(data_max_packet_size, static_cast<u16>(ep.max_packet_size & 0x7FF));
	}

	auto state = kstd::make_shared<RndisState>(
		device.device,
		comm_index,
		comm_in_ep,
		data_in_ep,
		data_out_ep,
		comm_info == &POLL_STATUS_BEFORE_CONTROL,
		data_max_packet_size);

	state->init();

	{
		IrqGuard irq_guard {};
		auto guard = NICS->lock();
		guard->push(state);
	}

	println("[kernel][usb]: rndis init done");

	dhcp_discover(&*state);
	state->run();
	return true;
}

static void* rndis_match(const usb::Config& config, const usb::DeviceDescriptor& device_desc) {
	bool comm_found = false;
	bool data_found = false;
	Info* info = nullptr;

	for (auto& interface : config.interfaces) {
		if (auto value = get_comm_interface(interface)) {
			comm_found = true;
			info = value;
		}
		else if (is_data_interface(interface)) {
			data_found = true;
		}
	}

	if (!comm_found || !data_found) {
		return nullptr;
	}
	else {
		return info;
	}
}

static constexpr UsbDriver RNDIS_DRIVER {
	.init = rndis_init,
	.fine_match = rndis_match
};

USB_DRIVER(RNDIS_DRIVER);
