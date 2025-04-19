#include "xhci.hpp"
#include "dev/clock.hpp"
#include "mem/mem.hpp"
#include "mem/vspace.hpp"
#include "sched/process.hpp"
#include "arch/irq.hpp"
#include "dev/usb/device.hpp"
#include "transfer_ring.hpp"

using namespace xhci;

namespace cap_regs {
	static constexpr BasicRegister<u8> CAPLENGTH {0x0};
	static constexpr BitRegister<u16> HCIVERSION {0x2};
	static constexpr BitRegister<u32> HCSPARAMS1 {0x4};
	static constexpr BitRegister<u32> HCSPARAMS2 {0x8};
	static constexpr BitRegister<u32> HCSPARAMS3 {0xC};
	static constexpr BitRegister<u32> HCCPARAMS1 {0x10};
	static constexpr BitRegister<u32> DBOFF {0x14};
	static constexpr BasicRegister<u32> RTSOFF {0x18};
	static constexpr BitRegister<u32> HCCPARAMS2 {0x1C};
}

namespace op_regs {
	static constexpr BitRegister<u32> USBCMD {0x0};
	static constexpr BitRegister<u32> USBSTS {0x4};
	static constexpr BitRegister<u32> PAGESIZE {0x8};
	static constexpr BitRegister<u32> DNCTRL {0x14};
	static constexpr BitRegister<u64> CRCR {0x18};
	static constexpr BitRegister<u64> DCBAAP {0x30};
	static constexpr BitRegister<u32> CONFIG {0x38};
}

namespace port_regs {
	static constexpr BitRegister<u32> PORTSC {0x0};
	static constexpr BitRegister<u32> PORTPMSC {0x4};
	static constexpr BitRegister<u32> PORTLI {0x8};
	static constexpr BitRegister<u32> PORTHLPMC {0xC};
}

namespace runtime_regs {
	static constexpr BitRegister<u32> MFINDEX {0x0};
	static constexpr BitRegister<u32> IMAN {0x0};
	static constexpr BitRegister<u32> IMOD {0x4};
	static constexpr BitRegister<u32> ERSTSZ {0x8};
	static constexpr BitRegister<u64> ERSTBA {0x10};
	static constexpr BitRegister<u64> ERDP {0x18};
}

namespace hcsparams1 {
	static constexpr BitField<u32, u8> MAX_SLOTS {0, 8};
	static constexpr BitField<u32, u16> MAX_INTRS {8, 11};
	static constexpr BitField<u32, u8> MAX_PORTS {24, 8};
}

namespace hcsparams2 {
	static constexpr BitField<u32, u8> ERST_MAX {4, 4};
	static constexpr BitField<u32, u8> MAX_SCRATCHPAD_BUFS_HI {21, 5};
	static constexpr BitField<u32, u8> MAX_SCRATCHPAD_BUFS_LO {27, 5};
}

namespace hccparams1 {
	static constexpr BitField<u32, bool> AC64 {0, 1};
	static constexpr BitField<u32, bool> CSZ {2, 1};
	static constexpr BitField<u32, u16> XECP {16, 16};
}

namespace hccparams2 {
	static constexpr BitField<u32, bool> CIC {5, 1};
}

namespace dboff {
	static constexpr BitField<u32, u32> DBOFF {2, 30};
}

namespace usbcmd {
	static constexpr BitField<u32, bool> RS {0, 1};
	static constexpr BitField<u32, bool> HCRST {1, 1};
	static constexpr BitField<u32, bool> INTE {2, 1};
	static constexpr BitField<u32, bool> HSEE {3, 1};
}

namespace usbsts {
	static constexpr BitField<u32, bool> HCH {0, 1};
	static constexpr BitField<u32, bool> HSE {2, 1};
	static constexpr BitField<u32, bool> EINT {3, 1};
	static constexpr BitField<u32, bool> PCD {4, 1};
	static constexpr BitField<u32, bool> CNR {11, 1};
	static constexpr BitField<u32, bool> HCE {12, 1};
}

namespace crcr {
	static constexpr BitField<u64, bool> RCS {0, 1};
	static constexpr BitField<u64, bool> CS {1, 1};
	static constexpr BitField<u64, bool> CA {2, 1};
	static constexpr BitField<u64, u64> CRP {6, 58};
}

namespace config {
	static constexpr BitField<u32, u8> MAX_SLOTS_EN {0, 8};
	static constexpr BitField<u32, bool> CIE {9, 1};
}

namespace xcap {
	static constexpr BitField<u32, u8> ID {0, 8};
	static constexpr BitField<u32, u8> NEXT {8, 8};

	static constexpr u8 USB_LEGACY_SUPPORT = 1;
	static constexpr u8 SUPPORTED_PROTOCOL = 2;
}

namespace usblegsup {
	static constexpr BitField<u32, bool> HC_BIOS_OWNED {16, 1};
	static constexpr BitField<u32, bool> HC_OS_OWNED {24, 1};
}

struct SlotContext {
	BitValue<u32> first {};
	u16 max_exit_latency {};
	u8 root_hub_port_number {};
	u8 number_of_ports {};
	BitValue<u32> second {};
	BitValue<u32> third {};
};

namespace slot_ctx {
	namespace first {
		static constexpr BitField<u32, u32> ROUTE_STRING {0, 20};
		static constexpr BitField<u32, u8> SPEED {20, 4};
		static constexpr BitField<u32, bool> HUB {26, 1};
		static constexpr BitField<u32, u8> CTX_ENTRIES {27, 5};
	}

	namespace second {
		static constexpr BitField<u32, u8> PARENT_HUB_SLOT_ID {0, 8};
		static constexpr BitField<u32, u8> PARENT_PORT_NUMBER {8, 8};
		static constexpr BitField<u32, u8> TT_THINK_TIME {16, 2};
		static constexpr BitField<u32, u8> INTERRUPTER_TARGET {22, 10};
	}

	namespace tr_dequeue_ptr {
		static constexpr BitField<u32, bool> DCS {0, 1};
	}

	namespace third {
		static constexpr BitField<u32, u8> USB_DEVICE_ADDRESS {0, 8};
		static constexpr BitField<u32, u8> SLOT_STATE {27, 5};
	}
}

struct EndpointContext {
	u32 first;
	u32 second;
	u32 tr_dequeue_ptr_low;
	u32 tr_dequeue_ptr_high;
	u32 third;
};

namespace endpoint_ctx {
	namespace first {
		static constexpr BitField<u32, u8> EP_STATE {0, 3};
		static constexpr BitField<u32, u8> MULT {8, 2};
		static constexpr BitField<u32, u8> MAX_P_STREAMS {10, 5};
		static constexpr BitField<u32, bool> LSA {15, 1};
		static constexpr BitField<u32, u8> INTERVAL {16, 8};
		static constexpr BitField<u32, u8> MAX_ESIT_PAYLOAD_HI {24, 8};
	}

	namespace second {
		static constexpr BitField<u32, u8> ERROR_COUNT {1, 2};
		static constexpr BitField<u32, u8> EP_TYPE {3, 3};
		static constexpr BitField<u32, bool> HID {7, 1};
		static constexpr BitField<u32, u8> MAX_BURST_SIZE {8, 8};
		static constexpr BitField<u32, u16> MAX_PACKET_SIZE {16, 16};
	}

	namespace tr_dequeue_ptr {
		static constexpr u64 DCS = 1;
	}

	namespace third {
		static constexpr BitField<u32, u16> AVG_TRB_LEN {0, 16};
		static constexpr BitField<u32, u16> MAX_ESIT_PAYLOAD_LO {16, 16};
	}

	static constexpr u8 EP_ISOCH_OUT = 1;
	static constexpr u8 EP_BULK_OUT = 2;
	static constexpr u8 EP_INTERRUPT_OUT = 3;
	static constexpr u8 EP_CONTROL = 4;
	static constexpr u8 EP_ISOCH_IN = 5;
	static constexpr u8 EP_BULK_IN = 6;
	static constexpr u8 EP_INTERRUPT_IN = 7;
}

namespace iman {
	static constexpr BitField<u32, bool> IP {0, 1};
	static constexpr BitField<u32, bool> IE {1, 1};
}

namespace imod {
	static constexpr BitField<u32, u16> IMODI {0, 16};
	static constexpr BitField<u32, u16> IMODC {16, 16};
}

namespace erstsz {
	static constexpr BitField<u32, u16> ERST_SIZE {0, 16};
}

namespace erstba {
	static constexpr BitField<u64, u64> ERST_BASE_ADDRESS {6, 58};
}

namespace erdp {
	static constexpr BitField<u64, u8> DESI {0, 3};
	static constexpr BitField<u64, bool> EHB {3, 1};
	static constexpr BitField<u64, u64> DEQUEUE_POINTER {4, 60};
}

namespace portsc {
	static constexpr BitField<u32, bool> CCS {0, 1};
	static constexpr BitField<u32, bool> PED {1, 1};
	static constexpr BitField<u32, bool> OCA {3, 1};
	static constexpr BitField<u32, bool> PR {4, 1};
	static constexpr BitField<u32, u8> PLS {5, 4};
	static constexpr BitField<u32, bool> PP {9, 1};
	static constexpr BitField<u32, u8> PORT_SPEED {10, 4};
	static constexpr BitField<u32, u8> PIC {14, 2};
	static constexpr BitField<u32, bool> LWS {16, 1};
	static constexpr BitField<u32, bool> CSC {17, 1};
	static constexpr BitField<u32, bool> PEC {18, 1};
	static constexpr BitField<u32, bool> WRC {19, 1};
	static constexpr BitField<u32, bool> OCC {20, 1};
	static constexpr BitField<u32, bool> PRC {21, 1};
	static constexpr BitField<u32, bool> PLC {22, 1};
	static constexpr BitField<u32, bool> CEC {23, 1};
	static constexpr BitField<u32, bool> CAS {24, 1};
}

namespace doorbell {
	static constexpr BitField<u32, u8> DB_TARGET {0, 8};
	static constexpr BitField<u32, u16> DB_STREAM_ID {16, 16};

	static constexpr u8 CONTROL_EP_ENQUEUE_PTR_UPDATE = 1;
	static constexpr u8 COMMAND_DOORBELL = 0;

	static constexpr u8 out_enqueue_ptr_update(u8 ep) {
		return 2 * (ep + 1);
	}

	static constexpr u8 in_enqueue_ptr_update(u8 ep) {
		return 2 * (ep + 1) + 1;
	}
}

namespace pls {
	static constexpr u8 U0 = 0;
	static constexpr u8 RX_DETECT = 5;
}

struct DeviceContextIndex {
	usize scratchpad_phys;
	usize device_context_phys[255];
};

struct InputControlContext {
	BitValue<u32> drop_context {};
	u32 add_context {};
	u32 reserved0[5] {};
	u8 configuration_value {};
	u8 interface_number {};
	u8 alternate_setting {};
	u8 reserved1 {};

	static constexpr BitField<u32, u32> DROP_CONTEXT {2, 30};
};

enum class CompletionCode : u8 {
	Invalid,
	Success,
	DataBufferError,
	BabbleDetected,
	TransactionError,
	TrbError,
	Stall,
	ResourceError,
	BandwidthError,
	NoSlotsAvailable,
	InvalidStreamType,
	SlotNotEnabled,
	EndpointNotEnabled,
	ShortPacket,
	RingUnderrun,
	RingOverrun,
	VfEventRingFull,
	ParameterError,
	BandwidthOverrun,
	ContextStateError,
	NoPingResponse,
	EventRingFull,
	IncompatibleDevice,
	MissedService,
	CommandRingStopped,
	CommandAborted,
	Stopped,
	StoppedLengthInvalid,
	StoppedShortPacket,
	MaxExitLatencyTooLarge,
	Reserved,
	IsochBufferOverrun,
	EventLost,
	UndefinedError,
	InvalidStreamId,
	SecondaryBandwidthError,
	SplitTransactionError
};

namespace {
	constexpr kstd::string_view status_to_str(CompletionCode code) {
		switch (code) {
			case CompletionCode::Invalid:
				return "invalid";
			case CompletionCode::Success:
				return "success";
			case CompletionCode::DataBufferError:
				return "data buffer error";
			case CompletionCode::BabbleDetected:
				return "babble detected";
			case CompletionCode::TransactionError:
				return "transaction error";
			case CompletionCode::TrbError:
				return "trb error";
			case CompletionCode::Stall:
				return "stall";
			case CompletionCode::ResourceError:
				return "resource error";
			case CompletionCode::BandwidthError:
				return "bandwidth error";
			case CompletionCode::NoSlotsAvailable:
				return "no slots available";
			case CompletionCode::InvalidStreamType:
				return "invalid stream type";
			case CompletionCode::SlotNotEnabled:
				return "slot not enabled";
			case CompletionCode::EndpointNotEnabled:
				return "endpoint not enabled";
			case CompletionCode::ShortPacket:
				return "short packet";
			case CompletionCode::RingUnderrun:
				return "ring underrrun";
			case CompletionCode::RingOverrun:
				return "ring overrun";
			case CompletionCode::VfEventRingFull:
				return "vf event ring full";
			case CompletionCode::ParameterError:
				return "parameter error";
			case CompletionCode::BandwidthOverrun:
				return "bandwidth overrun";
			case CompletionCode::ContextStateError:
				return "context state error";
			case CompletionCode::NoPingResponse:
				return "no ping response";
			case CompletionCode::EventRingFull:
				return "event ring full";
			case CompletionCode::IncompatibleDevice:
				return "incompatible device";
			case CompletionCode::MissedService:
				return "missed service";
			case CompletionCode::CommandRingStopped:
				return "command ring stopped";
			case CompletionCode::CommandAborted:
				return "command aborted";
			case CompletionCode::Stopped:
				return "stopped";
			case CompletionCode::StoppedLengthInvalid:
				return "stopped - length invalid";
			case CompletionCode::StoppedShortPacket:
				return "stopped - short packet";
			case CompletionCode::MaxExitLatencyTooLarge:
				return "max exit latency too large";
			case CompletionCode::Reserved:
				return "reserved";
			case CompletionCode::IsochBufferOverrun:
				return "isoch buffer overrun";
			case CompletionCode::EventLost:
				return "event lost";
			case CompletionCode::UndefinedError:
				return "undefined error";
			case CompletionCode::InvalidStreamId:
				return "invalid stream id";
			case CompletionCode::SecondaryBandwidthError:
				return "secondary bandwidth error";
			case CompletionCode::SplitTransactionError:
				return "split transaction error";
			default:
				return "unknown error";
		}
	}
}

namespace {
	struct InputContext {
		InputControlContext* control;
		SlotContext* slot;
		EndpointContext* ep0;
		struct {
			EndpointContext* in;
			EndpointContext* out;
		} endpoint[15];

		void init(bool big_context) {
			auto phys = pmalloc(1);
			assert(phys);
			memset(to_virt<void>(phys), 0, PAGE_SIZE);

			u32 ctx_size = big_context ? 64 : 32;
			control = to_virt<InputControlContext>(phys);
			slot = to_virt<SlotContext>(phys + ctx_size);
			ep0 = to_virt<EndpointContext>(phys + ctx_size * 2);
			u32 offset = ctx_size * 3;
			for (auto& ep : endpoint) {
				ep.out = to_virt<EndpointContext>(phys + offset);
				ep.in = to_virt<EndpointContext>(phys + offset + ctx_size);
				offset += ctx_size * 2;
			}
		}

		void free() {
			pfree(to_phys(control), 1);
			control = nullptr;
			slot = nullptr;
			ep0 = nullptr;
			for (auto& ep : endpoint) {
				ep.out = nullptr;
				ep.in = nullptr;
			}
		}

		[[nodiscard]] usize phys() const {
			return to_phys(control);
		}
	};

	constexpr usb::Status xhci_get_status(CompletionCode status) {
		if (status == CompletionCode::Success) {
			return usb::Status::Success;
		}
		else if (status == CompletionCode::ShortPacket) {
			return usb::Status::ShortPacket;
		}
		else {
			println("[kernel][xhci]: unhandled status ", status_to_str(status));
			return usb::Status::TransactionError;
		}
	}
}

struct XhciController;

struct Endpoint {
	volatile EndpointContext* ptr;
	IrqSpinlock<TransferRing*> ring;
};

struct DeviceContext final : public usb::Device {
	DeviceContext() {
		max_normal_packets = PAGE_SIZE / sizeof(TransferTrb) / 2 - 1;
	}

	void control_async(usb::setup::Packet* setup) override {
		auto guard = default_control_ring.lock();
		assert(!guard->events.find<usize, &usb::UsbEvent::trb_ptr>(setup->event.trb_ptr));
		guard->events.insert(&setup->event);

		guard->control(setup);
		trigger_control_ep();
	}

	bool normal_one(usb::normal::Packet* packet) override {
		assert(packet->ep < 15);
		auto& full_ep = endpoint[packet->ep];

		Endpoint* ep;
		if (packet->dir == usb::Dir::ToDevice) {
			ep = &full_ep.out;
		}
		else {
			ep = &full_ep.in;
		}

		auto guard = ep->ring.lock();
		assert(!(*guard)->events.find<usize, &usb::UsbEvent::trb_ptr>(packet->event.trb_ptr));

		if (!(*guard)->normal(packet)) {
			return false;
		}

		(*guard)->events.insert(&packet->event);

		if (packet->dir == usb::Dir::ToDevice) {
			trigger_out_ep(packet->ep);
		}
		else {
			trigger_in_ep(packet->ep);
		}

		return true;
	}

	usize normal_multiple(usb::normal::Packet* packets, usize count) override {
		for (usize i = 0; i < count; ++i) {
			auto* packet = &packets[i];
			if (!normal_one(packet)) {
				return i;
			}
		}

		return count;
	}

	usize normal_large(usb::normal::LargePacket* packet) override {
		assert(packet->ep < 15);
		auto& full_ep = endpoint[packet->ep];

		Endpoint* ep;
		if (packet->dir == usb::Dir::ToDevice) {
			ep = &full_ep.out;
		}
		else {
			ep = &full_ep.in;
		}

		auto guard = ep->ring.lock();
		assert(!(*guard)->events.find<usize, &usb::UsbEvent::trb_ptr>(packet->event.trb_ptr));

		auto count = (*guard)->normal_large(packet);

		(*guard)->events.insert(&packet->event);

		if (packet->dir == usb::Dir::ToDevice) {
			trigger_out_ep(packet->ep);
		}
		else {
			trigger_in_ep(packet->ep);
		}

		return count;
	}

	usb::Status set_config(const UniquePhysical& config) override;

	volatile SlotContext* slot {};
	volatile EndpointContext* control_ep {};
	struct {
		Endpoint in;
		Endpoint out;
	} endpoint[15] {};
	volatile u32* doorbell {};

	enum class State {
		Initial,
		Default,
		Addressed
	} state {};

	enum class Speed {
		Low,
		High,
		Full,
		Super,
		SuperPlus
	};

	u32 index {};
	u8 port {};
	Speed speed {};

	void reset(bool big_ctx) {
		memset(const_cast<SlotContext*>(slot), 0, big_ctx ? 2048 : 1024);
		state = State::Initial;
		port = 0;
		speed = Speed::Low;
		default_control_ring.lock()->reset();

		for (auto& ep : endpoint) {
			auto out_guard = ep.out.ring.lock();
			auto in_guard = ep.in.ring.lock();
			if (*out_guard) {
				delete *out_guard;
				*out_guard = nullptr;
			}
			if (*in_guard) {
				delete *in_guard;
				*in_guard = nullptr;
			}
		}
	}

	usb::DeviceDescriptor device_descriptor {};

	void trigger_control_ep() {
		*doorbell = doorbell::DB_TARGET(doorbell::CONTROL_EP_ENQUEUE_PTR_UPDATE);
	}

	void trigger_in_ep(u8 ep) {
		*doorbell = doorbell::DB_TARGET(doorbell::in_enqueue_ptr_update(ep));
	}

	void trigger_out_ep(u8 ep) {
		*doorbell = doorbell::DB_TARGET(doorbell::out_enqueue_ptr_update(ep));
	}

	IrqSpinlock<xhci::TransferRing> default_control_ring {};
	XhciController* controller {};
};

union CmdTrb {
	struct {
		u32 flags0;
		u32 flags1;
		u32 flags2;
		BitValue<u32> flags3;
	} generic;

	struct EnableSlot {
		u32 reserved[3];
		BitValue<u32> flags3;

		struct flags3 {
			static constexpr BitField<u32, u8> SLOT_TYPE {16, 5};
		};
	} enable_slot;

	struct DisableSlot {
		u32 reserved[3];
		BitValue<u32> flags3;

		struct flags3 {
			static constexpr BitField<u32, u8> SLOT_ID {24, 8};
		};
	} disable_slot;

	struct AddressDevice {
		u64 input_context_ptr;
		u32 reserved;
		BitValue<u32> flags3;

		struct flags3 {
			static constexpr BitField<u32, bool> BSR {9, 1};
			static constexpr BitField<u32, u8> SLOT_ID {24, 8};
		};
	} address_device;

	struct ConfigEndpoint {
		u64 input_context_ptr;
		u32 reserved;
		BitValue<u32> flags3;

		struct flags3 {
			static constexpr BitField<u32, u8> SLOT_ID {24, 8};
		};
	} config_endpoint;

	struct Link {
		u64 ring_segment_ptr;
		BitValue<u32> flags0;
		BitValue<u32> flags3;

		struct flags3 {
			static constexpr BitField<u32, bool> TC {1, 1};
			static constexpr BitField<u32, bool> CH {4, 1};
		};
	} link;

	struct flags3 {
		static constexpr BitField<u32, bool> C {0, 1};
		static constexpr BitField<u32, u8> TRB_TYPE {10, 6};
	};

	static constexpr u8 ENABLE_SLOT = 9;
	static constexpr u8 DISABLE_SLOT = 10;
	static constexpr u8 ADDRESS_DEVICE = 11;
	static constexpr u8 CONFIGURE_ENDPOINT = 12;
};
static_assert(sizeof(CmdTrb) == 16);

union EventTrb {
	enum Type : u8 {
		Transfer = 32,
		CommandCompletion = 33,
		PortStatusChange = 34,
		HostController = 37,
		DeviceNotification = 38,
		MifIndexWrap = 39
	};

	struct Generic {
		u32 flags0;
		u32 flags1;
		u32 flags2;
		BitValue<u32> flags3;

		struct flags3 {
			static constexpr BitField<u32, bool> C {0, 1};
			static constexpr BitField<u32, Type> TRB_TYPE {10, 6};
		};
	} generic;

	struct Transfer {
		u64 trb_pointer;
		BitValue<u32> flags0;
		BitValue<u32> flags1;

		struct flags0 {
			static constexpr BitField<u32, u32> TRANSFER_LEN {0, 24};
			static constexpr BitField<u32, CompletionCode> CODE {24, 8};
		};

		struct flags1 {
			static constexpr BitField<u32, bool> ED {2, 1};
			static constexpr BitField<u32, u8> ENDPOINT_ID {16, 5};
			static constexpr BitField<u32, u8> SLOT_ID {24, 8};
		};
	} transfer;

	struct CommandCompletion {
		u64 cmd_trb_ptr;
		BitValue<u32> flags0;
		BitValue<u32> flags1;

		struct flags0 {
			static constexpr BitField<u32, u32> PARAM {0, 24};
			static constexpr BitField<u32, CompletionCode> CODE {24, 8};
		};

		struct flags1 {
			static constexpr BitField<u32, u8> VF_ID {16, 8};
			static constexpr BitField<u32, u8> SLOT_ID {24, 8};
		};
	} cmd_completion;

	struct PortStatusChange {
		BitValue<u32> flags0;
		u32 reserved;
		BitValue<u32> flags1;
		BitValue<u32> flags2;

		struct flags0 {
			static constexpr BitField<u32, u8> PORT_ID {24, 8};
		};

		struct flags1 {
			static constexpr BitField<u32, CompletionCode> COMPLETION_CODE {24, 8};
		};
	} port_status_change;
};
static_assert(sizeof(EventTrb) == 16);

struct SupportedSpeed {
	u8 port_speed_id;
	u64 port_bit_rate;
};

struct SupportedProtocol {
	u8 major;
	u8 minor;
	u8 port_offset;
	u8 count;
	u8 protocol_slot_type;
	kstd::vector<SupportedSpeed> speeds;
};

struct XhciController {
	explicit XhciController(const GenericOps& ops, IoSpace space) : ops {ops} {
		cap_space = space;
		op_space = cap_space.subspace(cap_space.load(cap_regs::CAPLENGTH));
		runtime_space = cap_space.subspace(cap_space.load(cap_regs::RTSOFF));
	}

	void address_device(u8 slot_index, u8 port, u16 max_packet_size) {
		IoSpace port_space = op_space.subspace(0x400 + 0x10 * port);
		auto portsc = port_space.load(port_regs::PORTSC);
		auto port_speed = portsc & portsc::PORT_SPEED;

		DeviceContext::Speed slot_speed = DeviceContext::Speed::Low;
		if (!max_packet_size) {
			for (auto& protocol : supported_protocols) {
				if (port < protocol.port_offset ||
					port >= protocol.port_offset + protocol.count) {
					continue;
				}

				bool found = false;
				for (auto& speed : protocol.speeds) {
					if (port_speed == speed.port_speed_id) {
						found = true;

						if (speed.port_bit_rate <= 1000 * 1000 + (1000 * 1000 / 2)) {
							// low speed (1.5Mb/s)
							max_packet_size = 8;
							slot_speed = DeviceContext::Speed::Low;
						}
						else if (speed.port_bit_rate <= 1000 * 1000 * 12) {
							// full speed (12MB/s)
							max_packet_size = 64;
							slot_speed = DeviceContext::Speed::Full;
						}
						else if (speed.port_bit_rate <= 1000 * 1000 * 480) {
							// high speed (480Mb/s)
							max_packet_size = 64;
							slot_speed = DeviceContext::Speed::High;
						}
						else if (speed.port_bit_rate <= 1000UL * 1000 * 1000 * 5) {
							// super speed (gen1 x1) (5Gb/s)
							max_packet_size = 512;
							slot_speed = DeviceContext::Speed::Super;
						}
						else {
							max_packet_size = 512;
							slot_speed = DeviceContext::Speed::SuperPlus;
						}

						break;
					}
				}

				if (found) {
					break;
				}
			}
		}

		auto& slot = slots[slot_index];

		slot.default_control_ring.get_unsafe().max_packet_size = max_packet_size;
		slot.port = port;
		slot.speed = slot_speed;

		InputContext ctx {};
		ctx.init(ctx_size_flag);
		// modify slot context and ep0 context
		ctx.control->add_context |= 1 << 0 | 1 << 1;
		ctx.slot->root_hub_port_number = port + 1;
		ctx.slot->first = {slot_ctx::first::CTX_ENTRIES(1) | slot_ctx::first::SPEED(port_speed)};
		ctx.ep0->second =
			endpoint_ctx::second::EP_TYPE(endpoint_ctx::EP_CONTROL) |
			endpoint_ctx::second::MAX_PACKET_SIZE(max_packet_size) |
			endpoint_ctx::second::ERROR_COUNT(3);
		ctx.ep0->third =
			endpoint_ctx::third::AVG_TRB_LEN(8);
		ctx.ep0->tr_dequeue_ptr_low =
			slot.default_control_ring.get_unsafe().phys |
			endpoint_ctx::tr_dequeue_ptr::DCS;
		ctx.ep0->tr_dequeue_ptr_high = slot.default_control_ring.get_unsafe().phys >> 32;

		assert(slot.state == DeviceContext::State::Initial ||
		    slot.state == DeviceContext::State::Default);

		cmd_ring[cmd_ring_ptr++] = {
			.address_device {
				.input_context_ptr = ctx.phys(),
				.reserved {},
				.flags3 {
					CmdTrb::flags3::C(cmd_ring_c) |
					CmdTrb::flags3::TRB_TYPE(CmdTrb::ADDRESS_DEVICE) |
					CmdTrb::AddressDevice::flags3::BSR(slot.state == DeviceContext::State::Initial) |
					CmdTrb::AddressDevice::flags3::SLOT_ID(slot_index + 1)
				}
			}
		};

		if (cmd_ring_ptr == PAGE_SIZE / 16 - 1) {
			if (cmd_ring_c) {
				cmd_ring[cmd_ring_ptr].link.flags3 |= CmdTrb::flags3::C(true);
			}
			else {
				cmd_ring[cmd_ring_ptr].link.flags3 &= ~CmdTrb::flags3::C;
			}

			cmd_ring_ptr = 0;
			cmd_ring_c = !cmd_ring_c;
		}

		*host_controller_doorbell = doorbell::DB_TARGET(doorbell::COMMAND_DOORBELL);
	}

	void disable_slot(u8 slot) {
		cmd_ring[cmd_ring_ptr++] = {
			.disable_slot {
				.reserved {},
				.flags3 {
					CmdTrb::flags3::C(cmd_ring_c) |
					CmdTrb::flags3::TRB_TYPE(CmdTrb::DISABLE_SLOT) |
					CmdTrb::DisableSlot::flags3::SLOT_ID(slot + 1)
				}
			}
		};

		if (cmd_ring_ptr == PAGE_SIZE / 16 - 1) {
			if (cmd_ring_c) {
				cmd_ring[cmd_ring_ptr].link.flags3 |= CmdTrb::flags3::C(true);
			}
			else {
				cmd_ring[cmd_ring_ptr].link.flags3 &= ~CmdTrb::flags3::C;
			}

			cmd_ring_ptr = 0;
			cmd_ring_c = !cmd_ring_c;
		}

		*host_controller_doorbell = doorbell::DB_TARGET(doorbell::COMMAND_DOORBELL);
	}

	static constexpr u32 PORTSC_FLAGS =
		portsc::CSC(true) |
		portsc::PEC(true) |
		portsc::OCC(true) |
		portsc::PRC(true) |
		portsc::PLC(true);

	bool on_irq() {
		auto intr0_space = runtime_space.subspace(0x20 + (32 * 0));

		auto intr0_erdp = intr0_space.load(runtime_regs::ERDP);
		auto old = (intr0_erdp & erdp::DEQUEUE_POINTER) << 4;

		u32 index = (old - event_ring_segment_phys) / 16;
		auto& event = const_cast<EventTrb&>(event_ring[index]);

		auto type = event.generic.flags3 & EventTrb::Generic::flags3::TRB_TYPE;

		auto controller_guard = lock.lock();

		if (type == EventTrb::CommandCompletion) {
			auto status = event.cmd_completion.flags0 & EventTrb::CommandCompletion::flags0::CODE;
			auto slot_index = (event.cmd_completion.flags1 & EventTrb::CommandCompletion::flags1::SLOT_ID) - 1;

			u32 cmd_index = (event.cmd_completion.cmd_trb_ptr - cmd_ring_phys) / 16;
			u8 trb_type = cmd_ring[cmd_index].generic.flags3 & CmdTrb::flags3::TRB_TYPE;
			if (trb_type == CmdTrb::ENABLE_SLOT) {
				assert(status == CompletionCode::Success);

				u32 port = UINT32_MAX;
				for (size_t i = 0; i < slot_assign_queue.size(); ++i) {
					if (slot_assign_queue[i].second == cmd_index) {
						port = slot_assign_queue[i].first;
						slot_assign_queue.remove(i);
						break;
					}
				}

				assert(port != UINT32_MAX);

				ports[port].slot = slot_index;

				address_device(slot_index, port, 0);
			}
			else if (trb_type == CmdTrb::DISABLE_SLOT) {
				assert(status == CompletionCode::Success);

				println("[kernel][xhci]: disable slot ", slot_index);

				slots[slot_index].reset(ctx_size_flag);
			}
			else if (trb_type == CmdTrb::ADDRESS_DEVICE) {
				println("[kernel][xhci]: address device status: ", status_to_str(status));
				pfree(cmd_ring[cmd_index].address_device.input_context_ptr, 1);

				auto& slot = slots[slot_index];

				if (slot.state == DeviceContext::State::Initial) {
					println("[kernel][xhci]: waiting for trstrcy (50ms)");
					mdelay(50);

					auto page = pmalloc(1);
					assert(page);
					memset(to_virt<void>(page), 0, 8);

					usb::setup::Packet packet {
						usb::Dir::FromDevice,
						usb::setup::Type::Standard,
						usb::setup::Rec::Device,
						usb::setup::std_req::GET_DESCRIPTOR,
						usb::setup::desc_type::DEVICE << 8 | 0,
						0,
						page,
						static_cast<u16>(slot.default_control_ring.get_unsafe().max_packet_size)
					};

					auto guard = slot.default_control_ring.lock();
					guard->control(&packet);

					slot.state = DeviceContext::State::Default;
					slot.trigger_control_ep();
				}
				else {
					println("[kernel][xhci]: slot successfully addressed");
					slot.state = DeviceContext::State::Addressed;

					slot.attach();
				}
			}
			else if (trb_type == CmdTrb::CONFIGURE_ENDPOINT) {
				auto usb_event = cmd_events.find<usize, &usb::UsbEvent::trb_ptr>(slot_index);
				assert(usb_event);
				cmd_events.remove(usb_event);
				usb_event->len = 0;
				usb_event->status = xhci_get_status(status);
				usb_event->signal_one();
			}
			else {
				println("[kernel][xhci]: unimplemented trb type ", trb_type);
				assert(false);
			}
		}
		else if (type == EventTrb::Transfer) {
			auto status = event.transfer.flags0 & EventTrb::Transfer::flags0::CODE;
			auto len = event.transfer.flags0 & EventTrb::Transfer::flags0::TRANSFER_LEN;

			auto ep_index = (event.transfer.flags1 & EventTrb::Transfer::flags1::ENDPOINT_ID);
			auto slot_index = (event.transfer.flags1 & EventTrb::Transfer::flags1::SLOT_ID) - 1;

			auto& slot = slots[slot_index];

			if (slot.state == DeviceContext::State::Default) {
				if (status == CompletionCode::Success || status == CompletionCode::ShortPacket) {
					if (!(event.transfer.trb_pointer & 1UL << 63)) {
						auto* descriptor = to_virt<usb::DeviceDescriptor>(event.transfer.trb_pointer);
						slot.device_descriptor = *descriptor;
						pfree(event.transfer.trb_pointer, 1);

						IoSpace port_space = op_space.subspace(0x400 + 0x10 * slot.port);

						port_space.store(
							port_regs::PORTSC,
							portsc::PP(true) |
							PORTSC_FLAGS);
						port_space.store(port_regs::PORTSC, portsc::PP(true) | portsc::PR(true));
					}
				}
				else {
					println(
						"[kernel][xhci]: get descriptor failed with status ",
						status_to_str(status),
						", try replugging the device");
				}
			}
			else if ((event.transfer.flags1 & EventTrb::Transfer::flags1::ED) &&
				!(event.transfer.trb_pointer & 1UL << 63)) {
				auto trb_ptr = event.transfer.trb_pointer;

				if (ep_index == 1) {
					auto guard = slot.default_control_ring.lock();
					auto usb_event = guard->events.find<usize, &usb::UsbEvent::trb_ptr>(trb_ptr);
					assert(usb_event);
					guard->events.remove(usb_event);
					usb_event->len = len;
					usb_event->status = xhci_get_status(status);
					usb_event->signal_one();

					assert(usb_event->trb_count == 5);
					guard->used_count -= usb_event->trb_count;
				}
				else {
					auto& full_ep = slot.endpoint[ep_index / 2 - 1];
					auto& ep = ep_index % 2 ? full_ep.in : full_ep.out;

					auto guard = ep.ring.lock();

					auto usb_event = (*guard)->events.find<usize, &usb::UsbEvent::trb_ptr>(trb_ptr);
					assert(usb_event);
					(*guard)->events.remove(usb_event);
					usb_event->len = len;
					usb_event->status = xhci_get_status(status);
					usb_event->signal_one();

					(*guard)->used_count -= usb_event->trb_count;
				}
			}
		}
		else if (type == EventTrb::PortStatusChange) {
			u32 port_index = (event.port_status_change.flags0 & EventTrb::PortStatusChange::flags0::PORT_ID) - 1;
			auto& port = ports[port_index];

			IoSpace port_space = op_space.subspace(0x400 + 0x10 * port_index);

			auto portsc = port_space.load(port_regs::PORTSC);

			u32 flags = PORTSC_FLAGS | portsc::PP(true);

			bool reset = portsc & portsc::PRC;
			if (portsc & portsc::WRC) {
				flags |= portsc::WRC(true);
				reset = true;
			}
			port_space.store(port_regs::PORTSC, flags);

			if (portsc & portsc::CSC) {
				if (portsc & portsc::CCS) {
					if (port.initial_reset) {
						port_space.store(
							port_regs::PORTSC,
							portsc::PP(true) |
							portsc::PR(true));
					}
				}
				else {
					if (port.has_slot()) {
						auto slot = port.slot;
						port.slot = UINT16_MAX;
						disable_slot(slot);
					}
				}
			}
			else if (reset) {
				if ((portsc & portsc::PORT_SPEED) != 0) {
					if (!port.has_slot()) {
						println("[kernel][xhci]: connect on port ", port_index);

						u8 protocol_slot_type = 0;

						for (const auto& supported_protocol : supported_protocols) {
							if (port_index >= supported_protocol.port_offset &&
								port_index < supported_protocol.port_offset + supported_protocol.count) {
								protocol_slot_type = supported_protocol.protocol_slot_type;
								break;
							}
						}

						slot_assign_queue.push({port_index, cmd_ring_ptr});

						cmd_ring[cmd_ring_ptr++] = {
							.enable_slot {
								.reserved {},
								.flags3 {
									CmdTrb::flags3::C(cmd_ring_c) |
									CmdTrb::flags3::TRB_TYPE(CmdTrb::ENABLE_SLOT) |
									CmdTrb::EnableSlot::flags3::SLOT_TYPE(protocol_slot_type)
								}
							}
						};

						if (cmd_ring_ptr == PAGE_SIZE / 16 - 1) {
							if (cmd_ring_c) {
								cmd_ring[cmd_ring_ptr].link.flags3 |= CmdTrb::flags3::C(true);
							}
							else {
								cmd_ring[cmd_ring_ptr].link.flags3 &= ~CmdTrb::flags3::C;
							}

							cmd_ring_ptr = 0;
							cmd_ring_c = !cmd_ring_c;
						}

						*host_controller_doorbell = doorbell::DB_TARGET(doorbell::COMMAND_DOORBELL);
					}
					else {
						println("[kernel][xhci]: reset port ", port_index);
						assert(portsc & portsc::PED);

						auto& slot = slots[port.slot];

						if (slot.state == DeviceContext::State::Default) {
							if (!slot.device_descriptor.max_packet_size0) {
								slot.device_descriptor.max_packet_size0 = 3;
							}

							slot.default_control_ring.get_unsafe().reset();

							if (slot.speed >= DeviceContext::Speed::Super) {
								address_device(port.slot, slot.port, 1U << slot.device_descriptor.max_packet_size0);
							}
							else {
								address_device(port.slot, slot.port, slot.device_descriptor.max_packet_size0);
							}
						}
					}
				}
			}
		}
		else {
			println("[kernel][xhci]: unimplemented event trb type ", static_cast<int>(type));
			assert(false);
		}

		++index;
		if (index == PAGE_SIZE / 16) {
			index = 0;
		}

		intr0_erdp &= ~erdp::DEQUEUE_POINTER;
		intr0_erdp |= erdp::EHB(true);
		intr0_erdp |= erdp::DEQUEUE_POINTER((event_ring_segment_phys + index * 16) >> 4);
		intr0_space.store(runtime_regs::ERDP, intr0_erdp);

		return true;
	}

	bool init() {
		auto hccparams1 = cap_space.load(cap_regs::HCCPARAMS1);
		assert(hccparams1 & hccparams1::AC64);
		u32 extended_cap_offset = (hccparams1 & hccparams1::XECP) << 2;
		while (true) {
			BitValue<u32> value {cap_space.load<u32>(extended_cap_offset)};
			auto id = value & xcap::ID;
			if (id == xcap::USB_LEGACY_SUPPORT) {
				println("[kernel][xhci]: found legacy support cap");
				value |= usblegsup::HC_OS_OWNED(true);
				cap_space.store<u32>(extended_cap_offset, value);
				auto res = with_timeout([&]() {
					BitValue<u32> leg_sup {cap_space.load<u32>(extended_cap_offset)};
					return !(leg_sup & usblegsup::HC_BIOS_OWNED);
				}, US_IN_S);
				if (!res) {
					println("[kernel][xhci]: failed to acquire ownership from the bios");
					return false;
				}
			}
			else if (id == xcap::SUPPORTED_PROTOCOL) {
				u8 minor_revision = value >> 16;
				u8 major_revision = value >> 24;

				u32 name_str = cap_space.load<u32>(extended_cap_offset + 0x4);
				u32 dword1 = cap_space.load<u32>(extended_cap_offset + 0x8);
				u32 dword2 = cap_space.load<u32>(extended_cap_offset + 0xC);

				u8 compatible_port_offset = (dword1 & 0xFF) - 1;
				u8 compatible_port_count = dword1 >> 8;
				u8 protocol_speed_id_count = dword1 >> 28;
				u8 protocol_slot_type = dword2 & 0b11111;

				kstd::vector<SupportedSpeed> speeds;
				if (!protocol_speed_id_count) {
					if (major_revision == 2) {
						speeds.push(SupportedSpeed {
							.port_speed_id = 1,
							.port_bit_rate = 1000 * 1000 * 12
						});
						speeds.push(SupportedSpeed {
							.port_speed_id = 2,
							.port_bit_rate = 1000 * 1000 + (1000 * 1000 / 2)
						});
						speeds.push(SupportedSpeed {
							.port_speed_id = 3,
							.port_bit_rate = 1000 * 1000 * 480
						});
					}
					else if (major_revision == 3) {
						speeds.push(SupportedSpeed {
							.port_speed_id = 4,
							.port_bit_rate = 1000UL * 1000 * 1000 * 5
						});
						speeds.push(SupportedSpeed {
							.port_speed_id = 5,
							.port_bit_rate = 1000UL * 1000 * 1000 * 10
						});
						speeds.push(SupportedSpeed {
							.port_speed_id = 6,
							.port_bit_rate = 1000UL * 1000 * 1000 * 5
						});
						speeds.push(SupportedSpeed {
							.port_speed_id = 7,
							.port_bit_rate = 1000UL * 1000 * 1000 * 10
						});
					}
					else {
						assert(false);
					}
				}

				for (u32 i = 0; i < protocol_speed_id_count; ++i) {
					u32 dword = cap_space.load<u32>(extended_cap_offset + 0x10 + i * 4);

					u8 protocol_speed_id = dword & 0b1111;

					u8 protocol_speed_exponent = dword >> 4 & 0b11;

					u8 psi_type = dword >> 6 & 0b11;
					// if major_revision == 3
					u8 link_protocol = dword >> 14 & 0b11;

					u16 protocol_speed_mantissa = dword >> 16;

					u64 protocol_speed_multiplier;
					// bits/s
					if (protocol_speed_exponent == 0) {
						protocol_speed_multiplier = 1;
					}
					// Kb/s
					else if (protocol_speed_exponent == 1) {
						protocol_speed_multiplier = 1000;
					}
					// Mb/s
					else if (protocol_speed_exponent == 2) {
						protocol_speed_multiplier = 1000 * 1000;
					}
					// Gb/s
					else if (protocol_speed_exponent == 3) {
						protocol_speed_multiplier = 1000 * 1000 * 1000;
					}
					else {
						assert(false);
					}

					u64 protocol_speed = static_cast<u64>(protocol_speed_mantissa) * protocol_speed_multiplier;
					//println("[kernel][xhci]: supported speed: ", protocol_speed);

					speeds.push(SupportedSpeed {
						.port_speed_id = protocol_speed_id,
						.port_bit_rate = protocol_speed
					});
				}

				supported_protocols.push(SupportedProtocol {
					.major = major_revision,
					.minor = minor_revision,
					.port_offset = compatible_port_offset,
					.count = compatible_port_count,
					.protocol_slot_type = protocol_slot_type,
					.speeds {std::move(speeds)}
				});
			}

			u32 next_offset = (value & xcap::NEXT) << 2;
			if (!next_offset) {
				break;
			}
			extended_cap_offset += next_offset;
		}

		auto cmd = op_space.load(op_regs::USBCMD);
		cmd &= ~usbcmd::RS;
		op_space.store(op_regs::USBCMD, cmd);

		while (!(op_space.load(op_regs::USBSTS) & usbsts::HCH));

		cmd = op_space.load(op_regs::USBCMD);
		cmd |= usbcmd::HCRST(true);
		op_space.store(op_regs::USBCMD, cmd);

		while (op_space.load(op_regs::USBCMD) & usbcmd::HCRST);
		while (op_space.load(op_regs::USBSTS) & usbsts::CNR);

		auto hcsparams1 = cap_space.load(cap_regs::HCSPARAMS1);
		auto max_slots = hcsparams1 & hcsparams1::MAX_SLOTS;
		println("[kernel][xhci]: ", max_slots, " slots");
		slots.resize(max_slots);

		for (u32 i = 0; i < slots.size(); ++i) {
			slots[i].controller = this;
			slots[i].index = i;
		}

		auto hccparams2 = cap_space.load(cap_regs::HCCPARAMS2);

		auto config = op_space.load(op_regs::CONFIG);
		config &= ~config::MAX_SLOTS_EN;
		config |= config::MAX_SLOTS_EN(max_slots);

		if (hccparams2 & hccparams2::CIC) {
			config |= config::CIE(true);
		}

		op_space.store(op_regs::CONFIG, config);

		usize dcbaab_phys = pmalloc(1);
		assert(dcbaab_phys);
		op_space.store(op_regs::DCBAAP, dcbaab_phys);
		auto* device_ctx_index = to_virt<DeviceContextIndex>(dcbaab_phys);
		memset(device_ctx_index, 0, PAGE_SIZE);

		auto hcsparams2 = cap_space.load(cap_regs::HCSPARAMS2);
		u32 max_scratchpad_bufs = hcsparams2 & hcsparams2::MAX_SCRATCHPAD_BUFS_LO;
		max_scratchpad_bufs |= (hcsparams2 & hcsparams2::MAX_SCRATCHPAD_BUFS_HI) << 5;
		if (max_scratchpad_bufs) {
			usize scratchpad_index_phys = pmalloc(1);
			assert(scratchpad_index_phys);
			auto* scratchpad_index = to_virt<u64>(scratchpad_index_phys);
			memset(scratchpad_index, 0, PAGE_SIZE);

			device_ctx_index->scratchpad_phys = scratchpad_index_phys;

			for (u32 i = 0; i < max_scratchpad_bufs; ++i) {
				auto scratchpad_phys = pmalloc(1);
				assert(scratchpad_phys);
				memset(to_virt<void>(scratchpad_phys), 0, PAGE_SIZE);
				scratchpad_index[i] = scratchpad_phys;
			}
		}

		{
			usize prev_phys = 0;
			void* prev_virt = nullptr;
			u32 used = 0;
			u32 total_ctx_size;
			u32 single_size;

			if (hccparams1 & hccparams1::CSZ) {
				total_ctx_size = 2048;
				single_size = 64;
				ctx_size_flag = true;
			}
			else {
				total_ctx_size = 1024;
				single_size = 32;
			}

			auto* doorbells = offset(cap_space.mapping(), volatile u32*, (cap_space.load(cap_regs::DBOFF) & dboff::DBOFF) * 4);
			host_controller_doorbell = doorbells;

			u32 max = PAGE_SIZE / total_ctx_size;
			for (u32 i = 0; i < max_slots; ++i) {
				auto& slot = slots[i];
				slot.doorbell = &doorbells[1 + i];

				if (prev_phys) {
					device_ctx_index->device_context_phys[i] = prev_phys;

					slot.slot = static_cast<volatile SlotContext*>(prev_virt);
					prev_virt = offset(prev_virt, void*, single_size);
					slot.control_ep = static_cast<volatile EndpointContext*>(prev_virt);
					prev_virt = offset(prev_virt, void*, single_size);

					for (auto& ctx : slot.endpoint) {
						ctx.out.ptr = static_cast<volatile EndpointContext*>(prev_virt);
						prev_virt = offset(prev_virt, void*, single_size);
						ctx.in.ptr = static_cast<volatile EndpointContext*>(prev_virt);
						prev_virt = offset(prev_virt, void*, single_size);
					}

					prev_phys += total_ctx_size;
					++used;
					if (used == max) {
						prev_phys = 0;
						used = 0;
					}
				}
				else {
					auto phys = pmalloc(1);
					assert(phys);
					memset(to_virt<void>(phys), 0, PAGE_SIZE);

					device_ctx_index->device_context_phys[i] = phys;

					prev_virt = KERNEL_VSPACE.alloc(PAGE_SIZE);
					assert(prev_virt);
					assert(KERNEL_PROCESS->page_map.map(
						reinterpret_cast<u64>(prev_virt),
						phys,
						PageFlags::Read | PageFlags::Write,
						CacheMode::Uncached));

					slot.slot = static_cast<volatile SlotContext*>(prev_virt);
					prev_virt = offset(prev_virt, void*, single_size);
					slot.control_ep = static_cast<volatile EndpointContext*>(prev_virt);
					prev_virt = offset(prev_virt, void*, single_size);

					for (auto& ctx : slot.endpoint) {
						ctx.out.ptr = static_cast<volatile EndpointContext*>(prev_virt);
						prev_virt = offset(prev_virt, void*, single_size);
						ctx.in.ptr = static_cast<volatile EndpointContext*>(prev_virt);
						prev_virt = offset(prev_virt, void*, single_size);
					}

					prev_phys = phys + total_ctx_size;
					++used;
				}
			}
		}

		cmd_ring_phys = pmalloc(1);
		assert(cmd_ring_phys);
		cmd_ring = static_cast<CmdTrb*>(KERNEL_VSPACE.alloc(1));
		assert(cmd_ring);
		assert(KERNEL_PROCESS->page_map.map(
			reinterpret_cast<u64>(cmd_ring),
			cmd_ring_phys,
			PageFlags::Read | PageFlags::Write,
			CacheMode::Uncached));
		memset(cmd_ring, 0, PAGE_SIZE);

		cmd_ring[PAGE_SIZE / 16 - 1] = {
			.link {
				.ring_segment_ptr = cmd_ring_phys,
				.flags0 {},
				.flags3 {CmdTrb::flags3::TRB_TYPE(transfer_trb_type::LINK) | CmdTrb::Link::flags3::TC(true)}
			}
		};

		auto crcr = op_space.load(op_regs::CRCR);
		crcr &= ~crcr::CRP;
		crcr &= ~crcr::CS;
		crcr &= ~crcr::CA;
		crcr |= crcr::RCS(true);
		crcr |= crcr::CRP(cmd_ring_phys >> 6);
		op_space.store(op_regs::CRCR, crcr);

		op_space.store(op_regs::DNCTRL, op_space.load(op_regs::DNCTRL) | 1 << 1);

		auto intr0_space = runtime_space.subspace(0x20 + (32 * 0));

		auto intr0_erstsz = intr0_space.load(runtime_regs::ERSTSZ);
		intr0_erstsz &= ~erstsz::ERST_SIZE;
		intr0_erstsz |= erstsz::ERST_SIZE(1);
		intr0_space.store(runtime_regs::ERSTSZ, intr0_erstsz);

		auto erst_phys = pmalloc(1);
		assert(erst_phys);
		memset(to_virt<void>(erst_phys), 0, PAGE_SIZE);

		event_ring_segment_phys = pmalloc(1);
		assert(event_ring_segment_phys);
		auto* event_ring_segment = to_virt<void>(event_ring_segment_phys);
		memset(event_ring_segment, 0, PAGE_SIZE);

		event_ring = static_cast<volatile EventTrb*>(KERNEL_VSPACE.alloc(1));
		assert(event_ring);
		assert(KERNEL_PROCESS->page_map.map(
			reinterpret_cast<u64>(event_ring),
			event_ring_segment_phys,
			PageFlags::Read | PageFlags::Write,
			CacheMode::Uncached));

		*to_virt<u64>(erst_phys) = event_ring_segment_phys;
		*to_virt<u16>(erst_phys + 8) = PAGE_SIZE / 16;

		auto intr0_erdp = intr0_space.load(runtime_regs::ERDP);
		intr0_erdp &= ~erdp::DEQUEUE_POINTER;
		intr0_erdp &= ~erdp::EHB;
		intr0_erdp |= erdp::DEQUEUE_POINTER(event_ring_segment_phys >> 4);
		intr0_space.store(runtime_regs::ERDP, intr0_erdp);

		auto intr0_erstba = intr0_space.load(runtime_regs::ERSTBA);
		intr0_erstba &= ~erstba::ERST_BASE_ADDRESS;
		intr0_erstba |= erstba::ERST_BASE_ADDRESS(erst_phys >> 6);
		intr0_space.store(runtime_regs::ERSTBA, intr0_erstba);

		auto intr0_iman = intr0_space.load(runtime_regs::IMAN);
		intr0_iman |= iman::IE(true);
		intr0_space.store(runtime_regs::IMAN, intr0_iman);

		assert(ops.install_irq(ops.arg, &irq_handler));
		ops.enable_irqs(ops.arg, true);

		auto max_ports = hcsparams1 & hcsparams1::MAX_PORTS;

		ports.resize(max_ports);

		cmd = op_space.load(op_regs::USBCMD);
		cmd |= usbcmd::RS(true);
		cmd |= usbcmd::INTE(true);
		cmd |= usbcmd::HSEE(true);
		op_space.store(op_regs::USBCMD, cmd);

		while (op_space.load(op_regs::USBSTS) & usbsts::HCH);

		for (u32 i = 0; i < max_ports; ++i) {
			IoSpace port_space = op_space.subspace(0x400 + 0x10 * i);

			IrqGuard irq_guard {};
			port_space.store(
				port_regs::PORTSC,
				portsc::PP(true) |
				PORTSC_FLAGS);

			port_space.store(
				port_regs::PORTSC,
				portsc::PR(true) |
				portsc::PP(true));

			ports[i].initial_reset = true;
		}

		return true;
	}

	struct Port {
		bool initial_reset {};
		u16 slot {UINT16_MAX};

		constexpr Port() = default;
		constexpr Port(Port&&) {}

		[[nodiscard]] constexpr bool has_slot() const {
			return slot != UINT16_MAX;
		}
	};

	IoSpace cap_space;
	IoSpace op_space;
	IoSpace runtime_space;
	IrqHandler irq_handler {
		.fn = [&](IrqFrame*) {
			return on_irq();
		},
		.can_be_shared = false
	};
	GenericOps ops;
	kstd::vector<DeviceContext> slots {};
	kstd::vector<Port> ports {};
	kstd::vector<SupportedProtocol> supported_protocols {};
	kstd::vector<kstd::pair<u32, u32>> slot_assign_queue {};
	usize event_ring_segment_phys {};
	usize cmd_ring_phys {};
	volatile EventTrb* event_ring {};
	CmdTrb* cmd_ring {};
	RbTree<usb::UsbEvent, &usb::UsbEvent::hook> cmd_events {};
	volatile u32* host_controller_doorbell {};
	u32 cmd_ring_ptr {};
	bool cmd_ring_c {true};
	bool ctx_size_flag {};
	IrqSpinlock<void> lock {};
};

usb::Status DeviceContext::set_config(const UniquePhysical& config) {
	auto* config_desc = to_virt<usb::ConfigDescriptor>(config.base);

	IoSpace port_space = controller->op_space.subspace(0x400 + 0x10 * port);
	auto port_speed = port_space.load(port_regs::PORTSC) & portsc::PORT_SPEED;

	auto hccparams2 = controller->cap_space.load(cap_regs::HCCPARAMS2);

	InputContext ctx {};
	ctx.init(controller->ctx_size_flag);
	// modify slot context and ep0 context
	ctx.control->add_context |= 1 << 0;
	ctx.slot->root_hub_port_number = port + 1;

	u8 ctx_entries = 1;

	usb::EndpointDescriptor* last_ep = nullptr;
	usb::traverse_config(config, [&](u8 type, void* data, u8 length, usb::ConfigTraverseData& traverse_data) {
		if (type == usb::setup::desc_type::CONFIGURATION) {
			auto* config = static_cast<usb::ConfigDescriptor*>(data);
			if (hccparams2 & hccparams2::CIC) {
				ctx.control->configuration_value = config->config_value;
			}
		}
		else if (type == usb::setup::desc_type::ENDPOINT) {
			auto* ep = static_cast<usb::EndpointDescriptor*>(data);
			last_ep = ep;

			auto num = ep->number();
			auto ctx_index = 1 + num * 2 + 1;
			if (ep->dir_in()) {
				++ctx_index;
			}

			if (ctx_index > ctx_entries) {
				ctx_entries = ctx_index;
			}

			ctx.control->add_context |= 1U << ctx_index;

			auto& full_ep = ctx.endpoint[ep->number()];
			EndpointContext* ep_ctx = ep->dir_in() ? full_ep.in : full_ep.out;

			auto transfer_type = ep->transfer_type();

			// interval = 128us * pow(2, interval)

			u8 interval;
			if (transfer_type == usb::TransferType::Interrupt &&
				(speed == Speed::Low || speed == Speed::Full)) {
				u32 interval_blocks = ep->interval * 1000 / 125;
				interval = kstd::bit_width(interval_blocks) - 1;
			}
			else if (transfer_type == usb::TransferType::Isoch &&
				speed == Speed::Full) {
				u32 interval_blocks = (1U << ep->interval) * 1000 / 125;
				interval = kstd::bit_width(interval_blocks) - 1;
			}
			else if ((transfer_type == usb::TransferType::Bulk ||
				transfer_type == usb::TransferType::Control) &&
				(speed == Speed::Super || speed == Speed::SuperPlus)) {
				interval = 0;
			}
			else {
				interval = ep->interval;
			}

			ep_ctx->first = endpoint_ctx::first::INTERVAL(interval);

			u32 second = 0;
			if (transfer_type != usb::TransferType::Isoch) {
				second |= endpoint_ctx::second::ERROR_COUNT(3);
			}

			u8 ep_type;
			u16 avg_trb_len;
			switch (transfer_type) {
				case usb::TransferType::Control:
					ep_type = endpoint_ctx::EP_CONTROL;
					avg_trb_len = 8;
					break;
				case usb::TransferType::Isoch:
					if (ep->dir_in()) {
						ep_type = endpoint_ctx::EP_ISOCH_IN;
					}
					else {
						ep_type = endpoint_ctx::EP_ISOCH_OUT;
					}
					avg_trb_len = 1024 * 3;
					break;
				case usb::TransferType::Bulk:
					if (ep->dir_in()) {
						ep_type = endpoint_ctx::EP_BULK_IN;
					}
					else {
						ep_type = endpoint_ctx::EP_BULK_OUT;
					}
					avg_trb_len = 1024 * 3;
					break;
				case usb::TransferType::Interrupt:
					if (ep->dir_in()) {
						ep_type = endpoint_ctx::EP_INTERRUPT_IN;
					}
					else {
						ep_type = endpoint_ctx::EP_INTERRUPT_OUT;
					}
					avg_trb_len = 1024;
					break;
			}

			second |= endpoint_ctx::second::EP_TYPE(ep_type);

			u32 bursts = 0;
			if ((transfer_type == usb::TransferType::Isoch ||
				transfer_type == usb::TransferType::Interrupt) &&
				speed == Speed::High) {
				bursts = ep->max_packet_size >> 11 & 0b11;
				second |= endpoint_ctx::second::MAX_BURST_SIZE(bursts);
			}

			auto max_packet_size = ep->max_packet_size & 0x7FF;

			second |= endpoint_ctx::second::MAX_PACKET_SIZE(max_packet_size);

			auto max_esit = max_packet_size * (bursts + 1);
			u16 max_esit_low = max_esit > 0xFFFF ? 0xFFFF : max_esit;

			ep_ctx->second = second;
			ep_ctx->third = endpoint_ctx::third::AVG_TRB_LEN(avg_trb_len);

			if (transfer_type == usb::TransferType::Interrupt &&
				ep->irq_usage() == usb::IrqUsage::Periodic) {
				ep_ctx->third |= endpoint_ctx::third::MAX_ESIT_PAYLOAD_LO(max_esit_low);
			}

			auto& real_ep = ep->dir_in() ? endpoint[num].in : endpoint[num].out;
			real_ep.ring.get_unsafe() = new TransferRing {};

			ep_ctx->tr_dequeue_ptr_low =
				(*real_ep.ring.get_unsafe()).phys |
				endpoint_ctx::tr_dequeue_ptr::DCS;
			ep_ctx->tr_dequeue_ptr_high = (*real_ep.ring.get_unsafe()).phys >> 32;
		}
		else if (type == usb::setup::desc_type::SS_ENDPOINT_COMPANION) {
			auto* desc = static_cast<usb::SsEndpointCompanionDesc*>(data);
			assert(last_ep);

			auto& full_ep = ctx.endpoint[last_ep->number()];
			EndpointContext* ep_ctx = last_ep->dir_in() ? full_ep.in : full_ep.out;
			ep_ctx->second |= endpoint_ctx::second::MAX_BURST_SIZE(desc->max_burst);

			auto transfer_type = last_ep->transfer_type();

			if (transfer_type == usb::TransferType::Interrupt &&
				last_ep->irq_usage() == usb::IrqUsage::Periodic) {
				ep_ctx->third &= ~endpoint_ctx::third::MAX_ESIT_PAYLOAD_LO;
				ep_ctx->third |= endpoint_ctx::third::MAX_ESIT_PAYLOAD_LO(desc->bytes_per_interval);
			}
		}
	});

	ctx.slot->first = {slot_ctx::first::CTX_ENTRIES(ctx_entries) | slot_ctx::first::SPEED(port_speed)};

	usb::UsbEvent event {index, 1};

	{
		auto guard = controller->lock.lock();

		controller->cmd_events.insert(&event);

		controller->cmd_ring[controller->cmd_ring_ptr++] = {
			.config_endpoint {
				.input_context_ptr = ctx.phys(),
				.reserved {},
				.flags3 {
					CmdTrb::flags3::C(controller->cmd_ring_c) |
					CmdTrb::flags3::TRB_TYPE(CmdTrb::CONFIGURE_ENDPOINT) |
					CmdTrb::ConfigEndpoint::flags3::SLOT_ID(index + 1)
				}
			}
		};

		if (controller->cmd_ring_ptr == PAGE_SIZE / 16 - 1) {
			if (controller->cmd_ring_c) {
				controller->cmd_ring[controller->cmd_ring_ptr].link.flags3 |= CmdTrb::flags3::C(true);
			}
			else {
				controller->cmd_ring[controller->cmd_ring_ptr].link.flags3 &= ~CmdTrb::flags3::C;
			}

			controller->cmd_ring_ptr = 0;
			controller->cmd_ring_c = !controller->cmd_ring_c;
		}

		*controller->host_controller_doorbell = doorbell::DB_TARGET(doorbell::COMMAND_DOORBELL);
	}

	event.wait();

	if (event.status != usb::Status::Success) {
		println("[kernel][xhci]: set configuration failed with ", static_cast<u8>(event.status));
		return usb::Status::TransactionError;
	}

	auto status = control({
		usb::Dir::ToDevice,
		usb::setup::Type::Standard,
		usb::setup::Rec::Device,
		usb::setup::std_req::SET_CONFIGURATION,
		config_desc->config_value,
		0,
		reinterpret_cast<usize>(&event) & ~(1UL << 63),
		0
	});
	return status;
}

InitStatus xhci::init(const GenericOps& ops, IoSpace space) {
	auto controller = new XhciController {ops, space};
	if (!controller->init()) {
		delete controller;
		return InitStatus::Error;
	}

	return InitStatus::Success;
}
