#pragma once
#include "mem/register.hpp"

union TransferTrb {
	struct {
		u32 flags0;
		u32 flags1;
		u32 flags2;
		BitValue<u32> flags3;
	} generic;

	struct Normal {
		u64 data_ptr;
		BitValue<u32> flags0;
		BitValue<u32> flags3;

		struct flags0 {
			static constexpr BitField<u32, u32> TRANSFER_LEN {0, 17};
			static constexpr BitField<u32, u8> TD_SIZE {17, 5};
		};

		struct flags3 {
			static constexpr BitField<u32, bool> ENT {1, 1};
			static constexpr BitField<u32, bool> ISP {2, 1};
			static constexpr BitField<u32, bool> NS {3, 1};
			static constexpr BitField<u32, bool> CH {4, 1};
			static constexpr BitField<u32, bool> BEI {9, 1};
		};
	} normal;

	struct Setup {
		u8 m_request_type;
		u8 request;
		u16 value;
		u16 index;
		u16 length;
		BitValue<u32> flags0;
		BitValue<u32> flags3;

		struct flags0 {
			static constexpr BitField<u32, u32> TRANSFER_LEN {0, 17};
		};

		struct flags3 {
			static constexpr BitField<u32, u8> TRT {16, 2};
		};

		static constexpr u8 TRT_NO_DATA = 0;
		static constexpr u8 TRT_OUT_DATA = 2;
		static constexpr u8 TRT_IN_DATA = 3;

		static constexpr u8 TYPE_HOST_TO_DEV = 0;
		static constexpr u8 TYPE_DEV_TO_HOST = 1 << 7;
		static constexpr u8 TYPE_STANDARD = 0 << 5;
		static constexpr u8 TYPE_CLASS = 1 << 5;
		static constexpr u8 TYPE_VENDOR = 2 << 5;
		static constexpr u8 TYPE_REC_DEVICE = 0;
		static constexpr u8 TYPE_REC_INTERFACE = 1;
		static constexpr u8 TYPE_REC_ENDPOINT = 2;
	} setup;

	struct Data {
		u64 data_ptr;
		BitValue<u32> flags0;
		BitValue<u32> flags3;

		struct flags0 {
			static constexpr BitField<u32, u32> TRANSFER_LEN {0, 17};
			static constexpr BitField<u32, u8> TD_SIZE {17, 5};
		};

		struct flags3 {
			static constexpr BitField<u32, bool> ENT {1, 1};
			static constexpr BitField<u32, bool> ISP {2, 1};
			static constexpr BitField<u32, bool> NS {3, 1};
			static constexpr BitField<u32, bool> CH {4, 1};
			static constexpr BitField<u32, u8> DIR {16, 1};
		};

		static constexpr u8 DIR_OUT = 0;
		static constexpr u8 DIR_IN = 1;
	} data;

	struct Status {
		u64 reserved;
		BitValue<u32> flags0;
		BitValue<u32> flags3;

		struct flags3 {
			static constexpr BitField<u32, bool> ENT {1, 1};
			static constexpr BitField<u32, bool> CH {4, 1};
			static constexpr BitField<u32, u8> DIR {16, 1};
		};

		static constexpr u8 DIR_OUT = 0;
		static constexpr u8 DIR_IN = 1;
	} status;

	struct Isoch {
		u64 data_ptr;
		BitValue<u32> flags0;
		BitValue<u32> flags3;

		struct flags0 {
			static constexpr BitField<u32, u32> TRANSFER_LEN {0, 17};
			static constexpr BitField<u32, u8> TD_SIZE {17, 5};
		};

		struct flags3 {
			static constexpr BitField<u32, bool> ENT {1, 1};
			static constexpr BitField<u32, bool> ISP {2, 1};
			static constexpr BitField<u32, bool> NS {3, 1};
			static constexpr BitField<u32, bool> CH {4, 1};
			static constexpr BitField<u32, bool> IDT {6, 1};
			static constexpr BitField<u32, u8> TBC {7, 2};
			static constexpr BitField<u32, bool> BEI {9, 1};
			static constexpr BitField<u32, u8> TLBPC {16, 4};
			static constexpr BitField<u32, u16> FRAME_ID {20, 11};
			static constexpr BitField<u32, bool> SIA {31, 1};
		};
	} isoch;

	struct Noop {
		u64 reserved;
		BitValue<u32> flags0;
		BitValue<u32> flags3;

		struct flags3 {
			static constexpr BitField<u32, bool> ENT {1, 1};
			static constexpr BitField<u32, bool> CH {4, 1};
		};
	} noop;

	struct Link {
		u64 ring_segment_ptr;
		BitValue<u32> flags0;
		BitValue<u32> flags3;

		struct flags3 {
			static constexpr BitField<u32, bool> TC {1, 1};
			static constexpr BitField<u32, bool> CH {4, 1};
		};
	} link;

	struct EventData {
		u64 event_data;
		BitValue<u32> flags0;
		BitValue<u32> flags3;

		struct flags3 {
			static constexpr BitField<u32, bool> ENT {1, 1};
			static constexpr BitField<u32, bool> CH {4, 1};
			static constexpr BitField<u32, bool> BEI {9, 1};
		};
	} event_data;

	struct flags0 {
		static constexpr BitField<u32, u8> INTERRUPTER_TARGET {22, 10};
	};

	struct flags3 {
		static constexpr BitField<u32, bool> C {0, 1};
		static constexpr BitField<u32, bool> IOC {5, 1};
		static constexpr BitField<u32, bool> IDT {6, 1};
		static constexpr BitField<u32, u8> TRB_TYPE {10, 6};
	};
};
static_assert(sizeof(TransferTrb) == 16);

namespace transfer_trb_type {
	static constexpr u8 NORMAL = 1;
	static constexpr u8 SETUP = 2;
	static constexpr u8 DATA = 3;
	static constexpr u8 STATUS = 4;
	static constexpr u8 ISOCH = 5;
	static constexpr u8 LINK = 6;
	static constexpr u8 EVENT_DATA = 7;
	static constexpr u8 NOOP = 8;
}
