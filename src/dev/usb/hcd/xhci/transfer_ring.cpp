#include "transfer_ring.hpp"
#include "mem/pmalloc.hpp"
#include "mem/vspace.hpp"
#include "mem/mem.hpp"
#include "sched/process.hpp"
#include "assert.hpp"

using namespace xhci;

TransferRing::TransferRing() {
	phys = pmalloc(1);
	assert(phys);
	trbs = static_cast<TransferTrb*>(KERNEL_VSPACE.alloc(PAGE_SIZE));
	assert(trbs);
	assert(KERNEL_PROCESS->page_map.map(
		reinterpret_cast<u64>(trbs),
		phys,
		PageFlags::Read | PageFlags::Write,
		CacheMode::Uncached));
	memset(static_cast<void*>(trbs), 0, PAGE_SIZE);

	trbs[PAGE_SIZE / sizeof(TransferTrb) - 1] = {
		.link {
			.ring_segment_ptr = phys,
			.flags0 {},
			.flags3 {TransferTrb::flags3::TRB_TYPE(transfer_trb_type::LINK) |
					 TransferTrb::Link::flags3::TC(true)}
		}
	};
}

TransferRing::~TransferRing() {
	pfree(phys, 1);
}

void TransferRing::reset() {
	memset(trbs, 0, PAGE_SIZE);
	ring_ptr = 0;
	used_count = 0;
	ring_c = true;
}

bool TransferRing::add_trb(TransferTrb trb) {
	if (used_count == PAGE_SIZE / 16 - 1) {
		return false;
	}

	trb.generic.flags3 |= TransferTrb::flags3::C(ring_c);
	trbs[ring_ptr++] = trb;
	if (ring_ptr == PAGE_SIZE / 16 - 1) {
		if (ring_c) {
			trbs[ring_ptr].link.flags3 |= TransferTrb::flags3::C(true);
		}
		else {
			trbs[ring_ptr].link.flags3 &= ~TransferTrb::flags3::C;
		}

		ring_ptr = 0;
		ring_c = !ring_c;
	}

	++used_count;
	return true;
}

void TransferRing::control(const usb::setup::Packet* setup) {
	u8 trt;
	u8 data_dir;
	u8 status_dir;
	if (setup->request_type & static_cast<u8>(usb::Dir::FromDevice)) {
		trt = TransferTrb::Setup::TRT_IN_DATA;
		data_dir = TransferTrb::Data::DIR_IN;
		status_dir = TransferTrb::Status::DIR_OUT;
	}
	else {
		trt = TransferTrb::Setup::TRT_OUT_DATA;
		data_dir =  TransferTrb::Data::DIR_OUT;
		status_dir = TransferTrb::Status::DIR_IN;
	}

	TransferTrb setup_trb {
		.setup {
			.m_request_type = setup->request_type,
			.request = setup->request,
			.value = setup->value,
			.index = setup->index,
			.length = setup->length,
			.flags0 {TransferTrb::Setup::flags0::TRANSFER_LEN(8)},
			.flags3 {
				TransferTrb::flags3::TRB_TYPE(transfer_trb_type::SETUP) |
				TransferTrb::flags3::IDT(true) |
				TransferTrb::Setup::flags3::TRT(trt)
			}
		}
	};

	TransferTrb data_trb {
		.data {
			.data_ptr = setup->buffer,
			.flags0 {
				TransferTrb::Data::flags0::TRANSFER_LEN(setup->length)
			},
			.flags3 {
				TransferTrb::flags3::TRB_TYPE(transfer_trb_type::DATA) |
				TransferTrb::Data::flags3::DIR(data_dir) |
				TransferTrb::Data::flags3::CH(true) |
				TransferTrb::Data::flags3::ENT(true)
			}
		}
	};

	TransferTrb data_event_trb {
		.event_data {
			.event_data = setup->buffer,
			.flags0 {},
			.flags3 {
				TransferTrb::flags3::TRB_TYPE(transfer_trb_type::EVENT_DATA) |
				TransferTrb::flags3::IOC(true)
			}
		}
	};

	TransferTrb status_trb {
		.status {
			.reserved = 0,
			.flags0 {},
			.flags3 {
				TransferTrb::flags3::TRB_TYPE(transfer_trb_type::STATUS) |
				TransferTrb::Status::flags3::DIR(status_dir) |
				TransferTrb::Status::flags3::CH(true)
			}
		}
	};

	TransferTrb status_event_trb {
		.event_data {
			.event_data = 1UL << 63,
			.flags0 {},
			.flags3 {
				TransferTrb::flags3::TRB_TYPE(transfer_trb_type::EVENT_DATA) |
				TransferTrb::flags3::IOC(true)
			}
		}
	};

	add_trb(setup_trb);
	add_trb(data_trb);
	add_trb(data_event_trb);
	add_trb(status_trb);
	add_trb(status_event_trb);
}

bool TransferRing::normal(const usb::normal::Packet* packet) {
	if (used_count + 2 >= PAGE_SIZE / 16) {
		return false;
	}

	TransferTrb data_trb {
		.normal {
			.data_ptr = packet->buffer,
			.flags0 {
				TransferTrb::Normal::flags0::TRANSFER_LEN(packet->length)
			},
			.flags3 {
				TransferTrb::flags3::TRB_TYPE(transfer_trb_type::NORMAL) |
				TransferTrb::Normal::flags3::CH(true) |
				TransferTrb::Normal::flags3::ENT(true)
			}
		}
	};

	TransferTrb data_event_trb {
		.event_data {
			.event_data = packet->buffer,
			.flags0 {},
			.flags3 {
				TransferTrb::flags3::TRB_TYPE(transfer_trb_type::EVENT_DATA) |
				TransferTrb::flags3::IOC(true)
			}
		}
	};

	assert(add_trb(data_trb));
	assert(add_trb(data_event_trb));
	return true;
}

usize TransferRing::normal_large(const usb::normal::LargePacket* packet) {
	usize transferred = 0;
	for (usize i = 0; i < packet->count; ++i) {
		if (used_count + 2 >= PAGE_SIZE / 16) {
			break;
		}

		auto& transfer = packet->transfers[i];

		TransferTrb data_trb {
			.normal {
				.data_ptr = transfer.buffer,
				.flags0 {
					TransferTrb::Normal::flags0::TRANSFER_LEN(transfer.size)
				},
				.flags3 {
					TransferTrb::flags3::TRB_TYPE(transfer_trb_type::NORMAL) |
					TransferTrb::Normal::flags3::CH(true)
				}
			}
		};
		if (used_count + 3 >= PAGE_SIZE / 16 || i + 1 == packet->count) {
			data_trb.normal.flags3 |= TransferTrb::Normal::flags3::ENT(true);
		}
		else {
			u8 td_size = kstd::min(static_cast<usize>(packet->count - i - 1), static_cast<usize>(31));
			data_trb.normal.flags0 |= TransferTrb::Normal::flags0::TD_SIZE(td_size);
		}

		add_trb(data_trb);
		++transferred;
	}

	TransferTrb data_event_trb {
		.event_data {
			.event_data = packet->transfers->buffer,
			.flags0 {},
			.flags3 {
				TransferTrb::flags3::TRB_TYPE(transfer_trb_type::EVENT_DATA) |
				TransferTrb::flags3::IOC(true)
			}
		}
	};
	add_trb(data_event_trb);
	return transferred;
}
