#pragma once
#include "transfer_ring.hpp"
#include "dev/usb/packet.hpp"
#include "dev/event.hpp"
#include "rb_tree.hpp"
#include "compare.hpp"
#include "trb.hpp"

namespace xhci {
	struct TransferRing {
		TransferRing();

		~TransferRing();

		void reset();

		bool add_trb(TransferTrb trb);

		usize phys;
		TransferTrb* trbs;
		u32 ring_ptr {};
		u32 max_packet_size {};
		u32 used_count {};
		bool ring_c {true};

		void control(usb::setup::Packet setup, usize buffer);
		bool normal(const usb::normal::Packet* packet);

		RbTree<usb::UsbEvent, &usb::UsbEvent::hook> events {};
	};
}
