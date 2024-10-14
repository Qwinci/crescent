#pragma once
#include "types.hpp"
#include "dev/event.hpp"
#include "rb_tree.hpp"
#include "compare.hpp"

namespace usb {
	enum class Dir : u8 {
		ToDevice = 0 << 7,
		FromDevice = 1 << 7
	};

	enum class Status {
		Success,
		TransactionError,
		ShortPacket,
		Detached
	};

	struct UsbEvent : public Event {
		constexpr UsbEvent(usize trb_ptr, usize trb_count)
			: trb_ptr {trb_ptr}, trb_count {trb_count} {}

		RbTreeHook hook {};
		usize trb_ptr;
		usize trb_count;
		u32 len {};
		Status status {};

		int operator<=>(const UsbEvent& other) const {
			return kstd::threeway(trb_ptr, other.trb_ptr);
		}
	};

	namespace setup {
		enum class Type : u8 {
			Standard = 0 << 5,
			Class = 1 << 5,
			Vendor = 2 << 5
		};

		enum class Rec : u8 {
			Device,
			Interface,
			Endpoint,
			Other
		};

		namespace std_req {
			inline constexpr u8 GET_STATUS = 0;
			inline constexpr u8 CLEAR_FEATURE = 1;
			inline constexpr u8 SET_FEATURE = 3;
			inline constexpr u8 SET_ADDRESS = 5;
			inline constexpr u8 GET_DESCRIPTOR = 6;
			inline constexpr u8 SET_DESCRIPTOR = 7;
			inline constexpr u8 GET_CONFIGURATION = 8;
			inline constexpr u8 SET_CONFIGURATION = 9;
			inline constexpr u8 GET_INTERFACE = 10;
			inline constexpr u8 SET_INTERFACE = 11;
		}

		namespace desc_type {
			inline constexpr u8 DEVICE = 1;
			inline constexpr u8 CONFIGURATION = 2;
			inline constexpr u8 STRING = 3;
			inline constexpr u8 INTERFACE = 4;
			inline constexpr u8 ENDPOINT = 5;
			inline constexpr u8 INTERFACE_POWER = 8;
			inline constexpr u8 OTG = 9;
			inline constexpr u8 DEBUG = 10;
			inline constexpr u8 INTERFACE_ASSOCIATION = 11;
			inline constexpr u8 BOS = 15;
			inline constexpr u8 DEVICE_CAPABILITY = 16;
			inline constexpr u8 HID = 33;
			inline constexpr u8 SS_ENDPOINT_COMPANION = 48;
			inline constexpr u8 SSPLUS_ISOCH_ENDPOINT_COMPANION = 49;
		}

		struct Packet {
			constexpr Packet(
				Dir dir,
				Type type,
				Rec recipient,
				u8 request,
				u16 value,
				u16 index,
				usize buffer,
				u16 length)
					: event {buffer, 5}, buffer {buffer}, value {value},
					index {index}, length {length}, request {request} {
				request_type = static_cast<u8>(dir) |
					static_cast<u8>(type) |
					static_cast<u8>(recipient);
			}

			UsbEvent event;
			usize buffer;
			u16 value;
			u16 index;
			u16 length;
			u8 request_type;
			u8 request;
		};
	}

	namespace normal {
		struct Packet {
			constexpr Packet(Dir dir, u8 ep, usize buffer, u16 length)
				: event {buffer, 2}, buffer {buffer}, length {length}, ep {ep}, dir {dir} {}

			UsbEvent event;
			usize buffer;
			u16 length;
			u8 ep;
			Dir dir;
		};

		struct LargeTransfer {
			usize buffer;
			u16 size;
		};

		struct LargePacket {
			constexpr LargePacket(Dir dir, u8 ep, const LargeTransfer* transfers, usize count)
				: event {transfers->buffer, count + 1}, transfers {transfers}, count {count}, ep {ep}, dir {dir} {}

			UsbEvent event;
			const LargeTransfer* transfers;
			usize count;
			u8 ep;
			Dir dir;
		};
	}
}
