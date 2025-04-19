#include "arch/irq.hpp"
#include "dev/clock.hpp"
#include "mem/iospace.hpp"
#include "mem/mem.hpp"
#include "mem/vspace.hpp"
#include "sched/process.hpp"
#include "utils/driver.hpp"
#include "dev/sound/sound_dev.hpp"

namespace regs {
	static constexpr BitRegister<u16> GCAP {0};
	static constexpr BasicRegister<u8> VMIN {0x2};
	static constexpr BasicRegister<u8> VMAJ {0x3};
	static constexpr BitRegister<u16> OUTPAY {0x4};
	static constexpr BitRegister<u16> INPAY {0x6};
	static constexpr BitRegister<u32> GCTL {0x8};
	static constexpr BitRegister<u16> WAKEEN {0xC};
	static constexpr BitRegister<u16> STATESTS {0xE};
	static constexpr BitRegister<u16> GSTS {0x10};
	static constexpr BitRegister<u16> OUTSTRMPAY {0x18};
	static constexpr BitRegister<u16> INSTRMPAY {0x1A};
	static constexpr BitRegister<u32> INTCTL {0x20};
	static constexpr BitRegister<u32> INTSTS {0x24};
	static constexpr BasicRegister<u32> WALCLK {0x30};
	static constexpr BasicRegister<u32> SSYNC {0x38};
	static constexpr BasicRegister<u32> CORBLBASE {0x40};
	static constexpr BasicRegister<u32> CORBUBASE {0x44};
	static constexpr BitRegister<u16> CORBWP {0x48};
	static constexpr BitRegister<u16> CORBRP {0x4A};
	static constexpr BitRegister<u8> CORBCTL {0x4C};
	static constexpr BitRegister<u8> CORBSTS {0x4D};
	static constexpr BitRegister<u8> CORBSIZE {0x4E};
	static constexpr BasicRegister<u32> RIRBLBASE {0x50};
	static constexpr BasicRegister<u32> RIRBUBASE {0x54};
	static constexpr BitRegister<u16> RIRBWP {0x58};
	static constexpr BasicRegister<u16> RINTCNT {0x5A};
	static constexpr BitRegister<u8> RIRBCTL {0x5C};
	static constexpr BitRegister<u8> RIRBSTS {0x5D};
	static constexpr BitRegister<u8> RIRBSIZE {0x5E};
	static constexpr BitRegister<u32> ICOI {0x60};
	static constexpr BitRegister<u32> ICII {0x64};
	static constexpr BitRegister<u16> ICIS {0x68};
	static constexpr BitRegister<u32> DPLBASE {0x70};
	static constexpr BasicRegister<u32> DPUBASE {0x74};

	namespace stream {
		static constexpr BitRegister<u8> CTL0 {0};
		static constexpr BitRegister<u8> CTL1 {0x1};
		static constexpr BitRegister<u8> CTL2 {0x2};
		static constexpr BitRegister<u8> STS {0x3};
		static constexpr BasicRegister<u32> LPIB {0x4};
		static constexpr BasicRegister<u32> CBL {0x8};
		static constexpr BasicRegister<u16> LVI {0xC};
		static constexpr BasicRegister<u16> FIFOS {0x10};
		static constexpr BitRegister<u16> FMT {0x12};
		static constexpr BasicRegister<u32> BDPL {0x18};
		static constexpr BasicRegister<u32> BDPU {0x1C};
	}
}

namespace gcap {
	static constexpr BitField<u16, bool> OK64 {0, 1};
	static constexpr BitField<u16, u8> NSDO {1, 2};
	static constexpr BitField<u16, u8> BSS {3, 5};
	static constexpr BitField<u16, u8> ISS {8, 4};
	static constexpr BitField<u16, u8> OSS {12, 4};
}

namespace gctl {
	static constexpr BitField<u32, bool> CRST {0, 1};
	static constexpr BitField<u32, bool> FCNTRL {1, 1};
	static constexpr BitField<u32, bool> UNSOL {8, 1};
}

namespace intctl {
	static constexpr BitField<u32, u32> SIE {0, 30};
	static constexpr BitField<u32, bool> CIE {30, 1};
	static constexpr BitField<u32, bool> GIE {31, 1};
}

namespace intsts {
	static constexpr BitField<u32, u32> SIS {0, 30};
	static constexpr BitField<u32, bool> CIS {30, 1};
	static constexpr BitField<u32, bool> GIS {31, 1};
}

namespace corbwp {
	static constexpr BitField<u16, u8> WP {0, 8};
}

namespace corbrp {
	static constexpr BitField<u16, u8> RP {0, 8};
	static constexpr BitField<u16, bool> RST {15, 1};
}

namespace corbctl {
	static constexpr BitField<u8, bool> MEIE {0, 1};
	static constexpr BitField<u8, bool> RUN {1, 1};
}

namespace corbsize {
	static constexpr BitField<u8, u8> SIZE {0, 2};
	static constexpr BitField<u8, u8> SZCAP {4, 4};
}

namespace rirbwp {
	static constexpr BitField<u16, u8> WP {0, 8};
	static constexpr BitField<u16, bool> RST {15, 1};
}

namespace rirbctl {
	static constexpr BitField<u8, bool> INTCTL {0, 1};
	static constexpr BitField<u8, bool> DMAEN {1, 1};
	static constexpr BitField<u8, bool> OIC {2, 1};
}

namespace rirbsts {
	static constexpr BitField<u8, bool> INTFL {0, 1};
	static constexpr BitField<u8, bool> bois {2, 1};
}

namespace rirbsize {
	static constexpr BitField<u8, u8> SIZE {0, 2};
	static constexpr BitField<u8, u8> SZCAP {4, 4};
}

namespace dplbase {
	static constexpr BitField<u32, bool> DPBE {0, 1};
	static constexpr BitField<u32, u32> BASE {7, 25};
}

namespace sdctl0 {
	static constexpr BitField<u8, bool> RST {0, 1};
	static constexpr BitField<u8, bool> RUN {1, 1};
	static constexpr BitField<u8, bool> IOCE {2, 1};
	static constexpr BitField<u8, bool> FEIE {3, 1};
	static constexpr BitField<u8, bool> DEIE {4, 1};
}

namespace sdctl2 {
	static constexpr BitField<u8, u8> STRIPE {0, 2};
	static constexpr BitField<u8, bool> TP {2, 1};
	static constexpr BitField<u8, bool> DIR {3, 1};
	static constexpr BitField<u8, u8> STRM {4, 4};
}

namespace sdlvi {
	static constexpr BitField<u16, u8> LVI {0, 8};
}

namespace sdsts {
	static constexpr BitField<u8, bool> BCIS {2, 1};
	static constexpr BitField<u8, bool> FIFOE {3, 1};
	static constexpr BitField<u8, bool> DESE {4, 1};
	static constexpr BitField<u8, bool> FIFORDY {5, 1};
}

namespace sdfmt {
	static constexpr BitField<u16, u8> CHAN {0, 4};
	static constexpr BitField<u16, u8> BITS {4, 3};
	static constexpr BitField<u16, u8> DIV {8, 3};
	static constexpr BitField<u16, u8> MULT {11, 3};
	static constexpr BitField<u16, u8> BASE {14, 1};

	static constexpr u8 BASE_48KHZ = 0;
	static constexpr u8 BASE_441KHZ = 1;

	static constexpr u8 MULT_1 = 0b000;
	static constexpr u8 MULT_2 = 0b001;
	static constexpr u8 MULT_3 = 0b010;
	static constexpr u8 MULT_4 = 0b011;

	static constexpr u8 DIV_1 = 0b000;
	static constexpr u8 DIV_2 = 0b001;
	static constexpr u8 DIV_3 = 0b010;
	static constexpr u8 DIV_4 = 0b011;
	static constexpr u8 DIV_5 = 0b100;
	static constexpr u8 DIV_6 = 0b101;
	static constexpr u8 DIV_7 = 0b110;
	static constexpr u8 DIV_8 = 0b111;

	static constexpr u8 BITS_8 = 0b000;
	static constexpr u8 BITS_16 = 0b001;
	static constexpr u8 BITS_20 = 0b010;
	static constexpr u8 BITS_24 = 0b011;
	static constexpr u8 BITS_32 = 0b100;
}

namespace verb {
	static constexpr BitField<u32, u32> PAYLOAD {0, 20};
	static constexpr BitField<u32, u8> NODE_ID {20, 8};
	static constexpr BitField<u32, u8> CODEC_ADDRESS {28, 4};
}

namespace pcm_format {
	using namespace sdfmt;
	static constexpr BitField<u16, bool> PCM {15, 1};
}

namespace {
	struct BufferDescriptor {
		u64 address;
		u32 length;
		u32 ioc;
	};
	
	struct VerbDescriptor {
		BitValue<u32> value;

		constexpr void set_payload(u32 payload) {
			value |= verb::PAYLOAD(payload);
		}

		constexpr void set_nid(u8 nid) {
			value |= verb::NODE_ID(nid);
		}

		constexpr void set_cid(u8 cid) {
			value |= verb::CODEC_ADDRESS(cid);
		}
	};
	
	struct PcmFormat {
		BitValue<u16> value;

		constexpr u32 set_sample_rate(u32 rate) {
			u8 base = 0;
		    u8 mult = 0;
			u8 div = 0;
			u32 real_rate = 0;

			if (rate <= 5513) {
				real_rate = 5513;
				base = pcm_format::BASE_441KHZ;
				div = pcm_format::DIV_8;
			}
			else if (rate <= 6000) {
				real_rate = 6000;
				base = pcm_format::BASE_48KHZ;
				div = pcm_format::DIV_8;
			}
			else if (rate <= 6300) {
				real_rate = 6300;
				base = pcm_format::BASE_441KHZ;
				div = pcm_format::DIV_7;
			}
			else if (rate <= 6857) {
				real_rate = 6857;
				base = pcm_format::BASE_48KHZ;
				div = pcm_format::DIV_7;
			}
			else if (rate <= 7350) {
				real_rate = 7350;
				base = pcm_format::BASE_441KHZ;
				div = pcm_format::DIV_6;
			}
			else if (rate <= 8000) {
				real_rate = 8000;
				base = pcm_format::BASE_48KHZ;
				div = pcm_format::DIV_6;
			}
			else if (rate <= 8820) {
				real_rate = 8820;
				base = pcm_format::BASE_441KHZ;
				div = pcm_format::DIV_5;
			}
			else if (rate <= 9600) {
				real_rate = 9600;
				base = pcm_format::BASE_48KHZ;
				div = pcm_format::DIV_5;
			}
			else if (rate <= 11025) {
				real_rate = 11025;
				base = pcm_format::BASE_441KHZ;
				div = pcm_format::DIV_4;
			}
			else if (rate <= 12000) {
				real_rate = 12000;
				base = pcm_format::BASE_48KHZ;
				div = pcm_format::DIV_4;
			}
			else if (rate <= 12600) {
				real_rate = 12600;
				base = pcm_format::BASE_441KHZ;
				div = pcm_format::DIV_7;
				mult = pcm_format::MULT_2;
			}
			else if (rate <= 13714) {
				real_rate = 13714;
				base = pcm_format::BASE_48KHZ;
				div = pcm_format::DIV_7;
				mult = pcm_format::MULT_2;
			}
			else if (rate <= 14700) {
				real_rate = 14700;
				base = pcm_format::BASE_441KHZ;
				div = pcm_format::DIV_3;
			}
			else if (rate <= 16000) {
				real_rate = 16000;
				base = pcm_format::BASE_48KHZ;
				div = pcm_format::DIV_3;
			}
			else if (rate <= 16538) {
				real_rate = 16538;
				base = pcm_format::BASE_441KHZ;
				div = pcm_format::DIV_8;
				mult = pcm_format::MULT_3;
			}
			else if (rate <= 17640) {
				real_rate = 17640;
				base = pcm_format::BASE_441KHZ;
				div = pcm_format::DIV_5;
				mult = pcm_format::MULT_2;
			}
			else if (rate <= 18000) {
				real_rate = 18000;
				base = pcm_format::BASE_48KHZ;
				div = pcm_format::DIV_8;
				mult = pcm_format::MULT_3;
			}
			else if (rate <= 18900) {
				real_rate = 18900;
				base = pcm_format::BASE_441KHZ;
				div = pcm_format::DIV_7;
				mult = pcm_format::MULT_3;
			}
			else if (rate <= 19200) {
				real_rate = 19200;
				base = pcm_format::BASE_48KHZ;
				div = pcm_format::DIV_5;
				mult = pcm_format::MULT_2;
			}
			else if (rate <= 20571) {
				real_rate = 20571;
				base = pcm_format::BASE_48KHZ;
				div = pcm_format::DIV_7;
				mult = pcm_format::MULT_3;
			}
			else if (rate <= 22050) {
				real_rate = 22050;
				base = pcm_format::BASE_441KHZ;
				div = pcm_format::DIV_2;
			}
			else if (rate <= 24000) {
				real_rate = 24000;
				base = pcm_format::BASE_48KHZ;
				div = pcm_format::DIV_2;
			}
			else if (rate <= 25200) {
				real_rate = 25200;
				base = pcm_format::BASE_441KHZ;
				div = pcm_format::DIV_7;
				mult = pcm_format::MULT_4;
			}
			else if (rate <= 26460) {
				real_rate = 26460;
				base = pcm_format::BASE_441KHZ;
				div = pcm_format::DIV_5;
				mult = pcm_format::MULT_3;
			}
			else if (rate <= 27429) {
				real_rate = 27429;
				base = pcm_format::BASE_48KHZ;
				div = pcm_format::DIV_7;
				mult = pcm_format::MULT_4;
			}
			else if (rate <= 28800) {
				real_rate = 28800;
				base = pcm_format::BASE_48KHZ;
				div = pcm_format::DIV_5;
				mult = pcm_format::MULT_3;
			}
			else if (rate <= 29400) {
				real_rate = 29400;
				base = pcm_format::BASE_441KHZ;
				div = pcm_format::DIV_3;
				mult = pcm_format::MULT_2;
			}
			else if (rate <= 32000) {
				real_rate = 32000;
				base = pcm_format::BASE_48KHZ;
				div = pcm_format::DIV_3;
				mult = pcm_format::MULT_2;
			}
			else if (rate <= 33075) {
				real_rate = 33075;
				base = pcm_format::BASE_441KHZ;
				div = pcm_format::DIV_4;
				mult = pcm_format::MULT_3;
			}
			else if (rate <= 35280) {
				real_rate = 35280;
				base = pcm_format::BASE_441KHZ;
				div = pcm_format::DIV_5;
				mult = pcm_format::MULT_4;
			}
			else if (rate <= 36000) {
				real_rate = 36000;
				base = pcm_format::BASE_48KHZ;
				div = pcm_format::DIV_4;
				mult = pcm_format::MULT_3;
			}
			else if (rate <= 38400) {
				real_rate = 38400;
				base = pcm_format::BASE_48KHZ;
				div = pcm_format::DIV_5;
				mult = pcm_format::MULT_4;
			}
			else if (rate <= 44100) {
				real_rate = 44100;
				base = pcm_format::BASE_441KHZ;
			}
			else if (rate <= 48000) {
				real_rate = 48000;
				base = pcm_format::BASE_48KHZ;
			}
			else if (rate <= 58800) {
				real_rate = 58800;
				base = pcm_format::BASE_441KHZ;
				div = pcm_format::DIV_3;
				mult = pcm_format::MULT_4;
			}
			else if (rate <= 64000) {
				real_rate = 64000;
				base = pcm_format::BASE_48KHZ;
				div = pcm_format::DIV_3;
				mult = pcm_format::MULT_4;
			}
			else if (rate <= 66150) {
				real_rate = 66150;
				base = pcm_format::BASE_441KHZ;
				div = pcm_format::DIV_2;
				mult = pcm_format::MULT_3;
			}
			else if (rate <= 72000) {
				real_rate = 72000;
				base = pcm_format::BASE_48KHZ;
				div = pcm_format::DIV_2;
				mult = pcm_format::MULT_3;
			}
			else if (rate <= 88200) {
				real_rate = 88200;
				base = pcm_format::BASE_441KHZ;
				mult = pcm_format::MULT_2;
			}
			else if (rate <= 96000) {
				real_rate = 96000;
				base = pcm_format::BASE_48KHZ;
				mult = pcm_format::MULT_2;
			}
			else if (rate <= 132300) {
				real_rate = 132300;
				base = pcm_format::BASE_441KHZ;
				mult = pcm_format::MULT_3;
			}
			else if (rate <= 144000) {
				real_rate = 144000;
				base = pcm_format::BASE_48KHZ;
				mult = pcm_format::MULT_3;
			}
			else if (rate <= 176400) {
				real_rate = 176400;
				base = pcm_format::BASE_441KHZ;
				mult = pcm_format::MULT_4;
			}
			else {
				real_rate = 192000;
				base = pcm_format::BASE_48KHZ;
				mult = pcm_format::MULT_4;
			}

			value &= ~pcm_format::DIV;
			value &= ~pcm_format::MULT;
			value &= ~pcm_format::BASE;
			value |= pcm_format::DIV(div);
			value |= pcm_format::MULT(mult);
			value |= pcm_format::BASE(base);
			return real_rate;
		}

		constexpr u32 set_channels(u32 channels) {
			if (channels == 0) {
				channels = 1;
			}
			else if (channels > 16) {
				channels = 16;
			}

			value &= ~pcm_format::CHAN;
			value |= pcm_format::CHAN(channels - 1);
			return channels;
		}

		constexpr u32 set_bits_per_sample(u32 bits) {
			u8 bits_value = 0;
			if (bits <= 8) {
				bits = 8;
				bits_value = pcm_format::BITS_8;
			}
			else if (bits <= 16) {
				bits = 16;
				bits_value = pcm_format::BITS_16;
			}
			else if (bits <= 20) {
				bits = 20;
				bits_value = pcm_format::BITS_20;
			}
			else if (bits <= 24) {
				bits = 24;
				bits_value = pcm_format::BITS_24;
			}
			else {
				bits = 32;
				bits_value = pcm_format::BITS_32;
			}

			value &= ~pcm_format::BITS;
			value |= pcm_format::BITS(bits_value);
			return bits;
		}
	};
	
	struct ResponseDescriptor {
		u32 resp;
		u32 resp_ex;

		[[nodiscard]] constexpr u8 get_codec() const {
			return resp_ex & 0b1111;
		}

		[[nodiscard]] constexpr bool is_unsol() const {
			return resp_ex >> 4 & 1;
		}
	};
	
	struct StreamChannelPair {
		u8 channel;
		u8 stream;
	};
}

namespace cmd {
	enum : u16 {
		SET_CONVERTER_FORMAT = 0x2,
		SET_AMP_GAIN_MUTE = 0x3,
		GET_CONVERTER_FORMAT = 0xA,
		GET_AMP_GAIN_MUTE = 0xB,
		GET_PARAM = 0xF00,
		GET_CONN_SELECT = 0xF01,
		GET_CONN_LIST = 0xF02,
		GET_CONVERTER_CONTROL = 0xF06,
		GET_PIN_CONTROL = 0xF07,
		GET_EAPD_ENABLE = 0xF0C,
		GET_VOLUME_KNOB = 0xF0F,
		GET_CONFIG_DEFAULT = 0xF1C,
		GET_CONVERTER_CHANNEL_COUNT = 0xF2D,
		SET_CONN_SELECT = 0x701,
		SET_POWER_STATE = 0X705,
		SET_CONVERTER_CONTROL = 0x706,
		SET_PIN_CONTROL = 0x707,
		SET_EAPD_ENABLE = 0x70C,
		SET_VOLUME_KNOB = 0x70F,
		SET_CONVERTER_CHANNEL_COUNT = 0x72D
	};
}

namespace param {
	enum : u8 {
		NODE_COUNT = 0x4,
		FUNC_GROUP_TYPE = 0x5,
		AUDIO_CAPS = 0x9,
		PIN_CAPS = 0xC,
		IN_AMP_CAPS = 0xD,
		CONN_LIST_LEN = 0xE,
		OUT_AMP_CAPS = 0x12
	};
}

namespace func_group_type {
	enum : u8 {
		AUDIO = 0x1
	};
}

namespace widget_type {
	enum : u8 {
		AUDIO_OUT = 0x0,
		AUDIO_IN = 0x1,
		AUDIO_MIXER = 0x2,
		AUDIO_SELECTOR = 0x3,
		PIN_COMPLEX = 0x4,
		POWER_WIDGET = 0x5,
		VOLUME_KNOB_WIDGET = 0x6,
		BEEP_GENERATOR_WIDGET = 0x7
	};
}

static constexpr usize CHUNK_SIZE = 0x1000;
static constexpr usize MAX_DESCRIPTORS = PAGE_SIZE / sizeof(BufferDescriptor);

struct HdaStream {
	IoSpace space;
	kstd::atomic<usize> remaining_data;
	usize write_ptr {};
	Event event {};
	BufferDescriptor* bdl;

	void initialize_output() {
		auto bdl_phys = pmalloc(1);
		assert(bdl_phys);
		bdl = to_virt<BufferDescriptor>(bdl_phys);

		space.store(regs::stream::BDPL, bdl_phys);
		space.store(regs::stream::BDPU, bdl_phys >> 32);

		for (usize i = 0; i < MAX_DESCRIPTORS; ++i) {
			usize page = pmalloc(1);
			assert(page);
			bdl[i].address = page;
			bdl[i].length = PAGE_SIZE;
			if (i && (i * PAGE_SIZE) % CHUNK_SIZE == 0) {
				bdl[i].ioc = 1;
			}
			else {
				bdl[i].ioc = 0;
			}
		}

		space.store(regs::stream::CBL, MAX_DESCRIPTORS * PAGE_SIZE);

		auto lvi = space.load(regs::stream::LVI);
		lvi &= ~sdlvi::LVI;
		lvi |= sdlvi::LVI(MAX_DESCRIPTORS - 1);
		space.store(regs::stream::LVI, lvi);

		auto ctl2 = space.load(regs::stream::CTL2);
		ctl2 &= ~sdctl2::STRM;
		ctl2 |= sdctl2::STRM(1);
		space.store(regs::stream::CTL2, ctl2);

		auto ctl0 = space.load(regs::stream::CTL0);
		ctl0 |= sdctl0::IOCE(true);
		space.store(regs::stream::CTL0, ctl0);
	}

	void handle_output_irq() {
		usize old = remaining_data.load(kstd::memory_order::relaxed);;
		while (true) {
			u32 value;
			if (old < CHUNK_SIZE) {
				value = 0;
			}
			else {
				value = old - CHUNK_SIZE;
			}
			if (remaining_data.compare_exchange_weak(
					old,
					value,
					kstd::memory_order::relaxed,
					kstd::memory_order::relaxed)) {
				break;
			}
		}

		event.signal_one_if_not_pending();
	}
};

struct HdaController {
	explicit HdaController(pci::Device& device) {
		auto* virt = device.map_bar(0);
		assert(virt);
		space = IoSpace {virt};

		device.enable_mem_space(true);
		device.enable_bus_master(true);
		device.enable_legacy_irq(false);

		auto irq_count = device.alloc_irqs(1, 1, pci::IrqFlags::All);
		assert(irq_count);
		auto irq = device.get_irq(0);
		register_irq_handler(irq, &irq_handler);
		device.enable_irqs(true);

		device.set_power_state(pci::PowerState::D0);

		start();
	}

	void start();

	u8 submit_verb(u8 cid, u8 nid, u16 cmd, u8 data) {
		u8 index = (space.load(regs::CORBWP) & corbwp::WP) + 1;

		VerbDescriptor verb {};
		verb.set_cid(cid);
		verb.set_nid(nid);
		verb.set_payload(cmd << 8 | data);
		corb[index] = verb;

		auto corbwp_reg = space.load(regs::CORBWP);
		corbwp_reg &= ~corbwp::WP;
		corbwp_reg |= corbwp::WP(index);
		space.store(regs::CORBWP, corbwp_reg);

		return index;
	}

	u8 submit_verb_long(u8 cid, u8 nid, u8 cmd, u16 data) {
		u8 index = (space.load(regs::CORBWP) & corbwp::WP) + 1;

		VerbDescriptor verb {};
		verb.set_cid(cid);
		verb.set_nid(nid);
		verb.set_payload(cmd << 16 | data);
		corb[index] = verb;

		auto corbwp_reg = space.load(regs::CORBWP);
		corbwp_reg &= ~corbwp::WP;
		corbwp_reg |= corbwp::WP(index);
		space.store(regs::CORBWP, corbwp_reg);

		return index;
	}

	ResponseDescriptor wait_for_verb(u8 index) {
		while ((space.load(regs::CORBWP) & corbwp::WP) != index);
		auto success = with_timeout([&]() {
			auto rirbwp = space.load(regs::RIRBWP) & rirbwp::WP;
			return rirbwp == index;
		}, US_IN_S);
		if (!success) {
			panic("[kernel][hda]: waiting for verb ", index, " timed out");
		}
		return rirb[index];
	}

	bool handle_irq() {
		auto intsts = space.load(regs::INTSTS);
		auto streams = intsts & intsts::SIS;
		for (u32 i = 0; i < in_stream_count + out_stream_count; ++i) {
			if (streams & 1U << i) {
				if (i >= in_stream_count) {
					out_streams[i - in_stream_count].handle_output_irq();
				}
			}
		}

		return true;
	}

	IoSpace space;
	HdaStream in_streams[16] {};
	HdaStream out_streams[16] {};
	VerbDescriptor* corb {};
	ResponseDescriptor* rirb {};
	uint32_t* dma_current_pos {};
	IrqHandler irq_handler {
		.fn = [this](IrqFrame*) {
			return handle_irq();
		},
		.can_be_shared = false
	};
	u16 corb_size {};
	u16 rirb_size {};
	u8 in_stream_count {};
	u8 out_stream_count {};
};

struct HdaCodec;

struct HdaWidget {
	HdaCodec* codec;
	kstd::vector<u8> connections;
	u32 in_amp_caps;
	u32 out_amp_caps;
	u32 pin_caps;
	u32 default_config;
	u8 nid;
	u8 type;
};

struct HdaPath {
	kstd::vector<HdaWidget*> nodes;
};

struct HdaCodec {
	constexpr HdaCodec(HdaController* controller, u8 cid) : controller {controller}, cid {cid} {
		enumerate();
	}

	kstd::vector<HdaPath> find_paths() {
		kstd::vector<HdaPath> paths;

		struct StackEntry {
			HdaWidget& widget;
			u8 con_index;
			u8 con_range_index;
			u8 con_range_end;
		};

		kstd::vector<StackEntry> stack;

		for (auto pin_i : pin_complexes) {
			auto& pin = widgets[pin_i];
			// check if output capable
			if (!(pin.pin_caps & 1 << 4)) {
				continue;
			}
			u8 connectivity = pin.default_config >> 30;
			// no physical connection
			if (connectivity == 1) {
				continue;
			}

			stack.push({
				.widget = pin,
				.con_index = 0,
				.con_range_index = 0xFF,
				.con_range_end = 0
			});

			while (true) {
				if (stack.is_empty()) {
					break;
				}

				auto* cur_entry = stack.back();
				auto& cur_widget = cur_entry->widget;
				if (cur_entry->con_index == cur_widget.connections.size()) {
					stack.pop();
					continue;
				}

				if (cur_entry->con_range_index > cur_entry->con_range_end) {
					cur_entry->con_range_index = cur_widget.connections[cur_entry->con_index++];
					// the first connection can't be a range
					assert(cur_entry->con_range_index >> 7 == 0);
					if (cur_entry->con_index < cur_widget.connections.size() &&
						cur_widget.connections[cur_entry->con_index] & 1 << 7) {
						cur_entry->con_range_end = cur_widget.connections[cur_entry->con_index++] & 0x7F;
					}
					else {
						cur_entry->con_range_end = cur_entry->con_range_index;
					}
				}

				u8 nid = cur_entry->con_range_index++;
				assert(nid < widgets.size());
				auto& assoc_widget = widgets[nid];
				if (assoc_widget.type == widget_type::AUDIO_OUT) {
					HdaPath path {};
					for (auto& entry : stack) {
						path.nodes.push(&entry.widget);
					}
					path.nodes.push(&assoc_widget);
					paths.push(std::move(path));
				}
				else {
					bool circular_path = false;
					for (auto& entry : stack) {
						if (&entry.widget == &assoc_widget) {
							circular_path = true;
							break;
						}
					}

					if (circular_path || stack.size() >= 20) {
						continue;
					}
					stack.push({
						.widget = assoc_widget,
						.con_index = 0,
						.con_range_index = 0xFF,
						.con_range_end = 0
					});
				}
			}
		}

		return paths;
	}

	void enumerate() {
		auto num_func_groups_resp = get_parameter(0, param::NODE_COUNT);
		u8 num_func_groups = num_func_groups_resp & 0xFF;
		u8 func_groups_start_nid = num_func_groups_resp >> 16 & 0xFF;

		for (u8 func_group_i = func_groups_start_nid; func_group_i < func_groups_start_nid + num_func_groups; ++func_group_i) {
			auto func_group_type_resp = get_parameter(func_group_i, param::FUNC_GROUP_TYPE);
			if ((func_group_type_resp & 0xFF) != func_group_type::AUDIO) {
				continue;
			}

			set_power_state(func_group_i, 0);

			println("[kernel][hda]: audio function group found at ", cid, ":", func_group_i);

			auto num_widgets_resp = get_parameter(func_group_i, param::NODE_COUNT);
			u8 num_widgets = num_widgets_resp & 0xFF;
			u8 widgets_start_nid = num_widgets_resp >> 16 & 0xFF;

			println("[kernel][hda]: found ", num_widgets, " widgets");

			for (u8 widget_i = widgets_start_nid; widget_i < widgets_start_nid + num_widgets; ++widget_i) {
				auto audio_caps = get_parameter(widget_i, param::AUDIO_CAPS);
				auto in_amp_caps = get_parameter(widget_i, param::IN_AMP_CAPS);
				auto out_amp_caps = get_parameter(widget_i, param::OUT_AMP_CAPS);
				auto pin_caps = get_parameter(widget_i, param::PIN_CAPS);
				auto conn_list_len_resp = get_parameter(widget_i, param::CONN_LIST_LEN);
				auto default_config = get_config_default(widget_i);
				u8 type = audio_caps >> 20 & 0b1111;

				assert(!(conn_list_len_resp & 1 << 7) && "long form connection lists are not supported");

				kstd::vector<u8> connections;
				u8 conn_list_len = conn_list_len_resp & 0x7F;
				connections.reserve(conn_list_len);
				for (u8 i = 0; i < conn_list_len; i += 4) {
					u32 resp = get_connection_list(widget_i, i);
					for (u8 j = 0; j < kstd::min(conn_list_len - i, 4); ++j) {
						u8 nid = resp >> (j * 8) & 0xFF;
						connections.push(nid);
					}
				}

				HdaWidget widget {
					.codec = this,
					.connections {std::move(connections)},
					.in_amp_caps = in_amp_caps,
					.out_amp_caps = out_amp_caps,
					.pin_caps = pin_caps,
					.default_config = default_config,
					.nid = widget_i,
					.type = type
				};
				if (widgets.size() <= widget_i) {
					widgets.resize(widget_i + 1);
				}
				widgets[widget_i] = widget;

				if (type == widget_type::AUDIO_OUT) {
					audio_outputs.push(widget_i);
				}
				else if (type == widget_type::PIN_COMPLEX) {
					pin_complexes.push(widget_i);
				}
			}
		}

		println("found ", audio_outputs.size(), " audio outputs and ", pin_complexes.size(), " pin complexes");

		output_paths = find_paths();

		kstd::vector<u8> visited_assocs;

		for (auto pin_i : pin_complexes) {
			auto pin = &widgets[pin_i];

			u8 assoc = pin->default_config >> 4 & 0b1111;
			u8 connection_type = pin->default_config >> 16 & 0b1111;
			u8 default_dev = pin->default_config >> 20 & 0b1111;
			u8 location = pin->default_config >> 24 & 0b111111;
			u8 connectivity = pin->default_config >> 30;

			if (default_dev == 0 &&
				(connectivity == 0b10 || connectivity == 0b11)) {
				default_dev = 1;
			}

			switch (default_dev) {
				case 0:
				case 1:
				case 2:
				case 4:
				case 5:
				{
					if (!(pin->pin_caps & 1 << 4)) {
						println("pin is not output capable");
						continue;
					}
					break;
				}
				default:
					break;
			}

			bool assoc_found = false;
			for (auto visited_assoc : visited_assocs) {
				if (visited_assoc == assoc) {
					assoc_found = true;
					break;
				}
			}

			if (assoc_found) {
				println("pin with same assoc");
				continue;
			}

			if (!assoc) {
				println("pin with no assoc");
				continue;
			}

			visited_assocs.push(assoc);

			const char* connectivity_str = "";
			switch (connectivity) {
				case 0:
					connectivity_str = "jack";
					break;
				case 0b10:
					connectivity_str = "integrated";
					break;
				case 0b11:
					connectivity_str = "jack + integrated";
					break;
				default:
					break;
			}

			u8 location_low = location & 0b1111;
			u8 location_high = location >> 4 & 0b11;

			const char* location_coarse = "";
			switch (location_low) {
				case 0:
					location_coarse = "n/a";
					break;
				case 1:
					location_coarse = "rear";
					break;
				case 2:
					location_coarse = "front";
					break;
				case 3:
					location_coarse = "left";
					break;
				case 4:
					location_coarse = "right";
					break;
				case 5:
					location_coarse = "top";
					break;
				case 6:
					location_coarse = "bottom";
					break;
				case 7:
				case 8:
				case 9:
					location_coarse = "special";
					break;
				default:
					break;
			}

			const char* location_fine = "";
			switch (location_high) {
				case 0:
					location_fine = "external";
					break;
				case 1:
					location_fine = "internal";
					break;
				case 0b10:
					location_fine = "separate chassis";
					break;
				case 0b11:
					location_fine = "other";
					break;
				default:
					break;
			}

			const char* device = "unknown";
			switch (default_dev) {
				case 0:
					device = "line out";
					break;
				case 1:
					device = "speaker";
					break;
				case 2:
					device = "hp out";
					break;
				case 3:
					device = "cd";
					break;
				case 4:
					device = "spdif out";
					break;
				case 5:
					device = "digital other out";
					break;
				case 6:
					device = "modem line side";
					break;
				case 7:
					device = "modem handset side";
					break;
				case 8:
					device = "line in";
					break;
				case 9:
					device = "aux";
					break;
				case 10:
					device = "mic in";
					break;
				case 11:
					device = "telephony";
					break;
				case 12:
					device = "spdif in";
					break;
				case 13:
					device = "digital other in";
					break;
				case 15:
					device = "other";
					break;
				default:
					break;
			}

			const char* connection_type_str = "";
			switch (connection_type) {
				case 0:
					connection_type_str = "unknown";
					break;
				case 1:
					connection_type_str = "1/8\" stereo/mono";
					break;
				case 2:
					connection_type_str = "1/4\" stereo/mono";
					break;
				case 3:
					connection_type_str = "atapi internal";
					break;
				case 4:
					connection_type_str = "rca";
					break;
				case 5:
					connection_type_str = "optical";
					break;
				case 6:
					connection_type_str = "other digital";
					break;
				case 7:
					connection_type_str = "other analog";
					break;
				case 8:
					connection_type_str = "multichannel analog";
					break;
				case 9:
					connection_type_str = "xlr/professional";
					break;
				case 10:
					connection_type_str = "rj-11 modem";
					break;
				case 11:
					connection_type_str = "combination";
					break;
				case 15:
					connection_type_str = "other";
					break;
			}

			println("[kernel][hda]: pin ", location_fine, " ", location_coarse, " ", connectivity_str, " ", device, " ", connection_type_str, device);
		}
	}

	void set_active_path(const HdaPath& path) {
		for (usize i = 0; i < path.nodes.size(); ++i) {
			auto widget = path.nodes[i];
			if (i != path.nodes.size() - 1) {
				auto next_widget = path.nodes[i + 1];

				usize index = 0;
				for (usize j = 0; j < widget->connections.size(); ++j) {
					auto connection = widget->connections[j];
					if (connection & 1 << 7) {
						auto start = widget->connections[j - 1];
						auto end = connection & 0x7F;
						if (next_widget->nid >= start && next_widget->nid <= end) {
							index += next_widget->nid - start;
							break;
						}
						index += end - start;
					}
					else {
						if (next_widget->nid == connection) {
							break;
						}
						index += 1;
					}
				}

				set_selected_connection(widget->nid, index);
			}

			if (widget->type == widget_type::PIN_COMPLEX) {
				if (widget->pin_caps & 1 << 16) {
					set_eapd_enable(widget->nid, 1 << 1);
				}

				u8 step = widget->out_amp_caps & 0x7F;

				// set output amp, set left amp, set right amp and gain
				u16 amp_data = 1 << 15 | 1 << 13 | 1 << 12 | step;
				set_amp_gain_mute(widget->nid, amp_data);
				set_power_state(widget->nid, 0);
				// headphone amp, out enable
				u32 pin_control = 1 << 7 | 1 << 6;
				set_pin_control(widget->nid, pin_control);
			}
			else if (widget->type == widget_type::AUDIO_MIXER) {
				u8 step = widget->out_amp_caps & 0x7F;

				// set output amp, set left amp, set right amp and gain
				u16 amp_data = 1 << 15 | 1 << 13 | 1 << 12 | step;
				set_amp_gain_mute(widget->nid, amp_data);
				set_power_state(widget->nid, 0);
			}
			else if (widget->type == widget_type::AUDIO_OUT) {
				set_converter_control(
					widget->nid,
					{.channel = 0, .stream = 1});
				set_power_state(widget->nid, 0);
			}
		}
	}

	void set_volume(const HdaPath& path, u8 percentage) {
		auto& output = path.nodes[path.nodes.size() - 1];
		assert(output->type == widget_type::AUDIO_OUT);

		u8 max_value = output->out_amp_caps & 0x7F;
		u8 one_percentage = max_value / 100;
		if (one_percentage == 0) {
			one_percentage = 1;
		}
		u32 value = one_percentage * percentage;
		if (value > max_value || percentage == 100) {
			value = max_value;
		}

		// set output amp, set left amp, set right amp and gain
		u16 amp_data = 1 << 15 | 1 << 13 | 1 << 12 | value;
		set_amp_gain_mute(output->nid, amp_data);
	}

	void set_format(const HdaPath& path, PcmFormat fmt) {
		auto& output = path.nodes[path.nodes.size() - 1];
		assert(output->type == widget_type::AUDIO_OUT);
		set_converter_format(output->nid, fmt);
		set_converter_channel_count(output->nid, fmt.value & pcm_format::CHAN);
	}

	[[nodiscard]] u32 get_parameter(u8 nid, u8 param) const {
		auto index = controller->submit_verb(cid, nid, cmd::GET_PARAM, param);
		return controller->wait_for_verb(index).resp;
	}

	[[nodiscard]] u8 get_selected_connection(u8 nid) const {
		auto index = controller->submit_verb(cid, nid, cmd::GET_CONN_SELECT, 0);
		return controller->wait_for_verb(index).resp & 0xFF;
	}

	void set_selected_connection(u8 nid, u8 index) const {
		auto submit_index = controller->submit_verb(cid, nid, cmd::SET_CONN_SELECT, index);
		controller->wait_for_verb(submit_index);
	}

	[[nodiscard]] u32 get_connection_list(u8 nid, u8 offset_index) const {
		auto index = controller->submit_verb(cid, nid, cmd::GET_CONN_LIST, offset_index);
		return controller->wait_for_verb(index).resp;
	}

	[[nodiscard]] u32 get_amp_gain_mute(u8 nid, bool out_amp, bool left, u8 index) const {
		u16 data = (out_amp ? 1 << 15 : 0) | (left ? 1 << 13 : 0) | (index & 0xF);
		auto submit_index = controller->submit_verb_long(cid, nid, cmd::GET_AMP_GAIN_MUTE, data);
		return controller->wait_for_verb(submit_index).resp;
	}

	void set_amp_gain_mute(u8 nid, u16 data) const {
		auto index = controller->submit_verb_long(cid, nid, cmd::SET_AMP_GAIN_MUTE, data);
		controller->wait_for_verb(index);
	}

	[[nodiscard]] PcmFormat get_converter_format(u8 nid) const {
		auto index = controller->submit_verb_long(cid, nid, cmd::GET_CONVERTER_FORMAT, 0);
		return {.value {static_cast<u16>(controller->wait_for_verb(index).resp)}};
	}

	void set_converter_format(u8 nid, PcmFormat format) const {
		auto index = controller->submit_verb_long(cid, nid, cmd::SET_CONVERTER_FORMAT, format.value);
		controller->wait_for_verb(index);
	}

	[[nodiscard]] StreamChannelPair get_converter_control(u8 nid) const {
		auto index = controller->submit_verb(cid, nid, cmd::GET_CONVERTER_CONTROL, 0);
		auto resp =  controller->wait_for_verb(index);
		u8 channel = resp.resp & 0xF;
		u8 stream = resp.resp >> 4 & 0xF;
		return {.channel = channel, .stream = stream};
	}

	void set_converter_control(u8 nid, StreamChannelPair stream) const {
		auto index = controller->submit_verb(cid, nid, cmd::SET_CONVERTER_CONTROL, stream.channel | stream.stream << 4);
		controller->wait_for_verb(index);
	}

	[[nodiscard]] u8 get_pin_control(u8 nid) const {
		auto index = controller->submit_verb(cid, nid, cmd::GET_PIN_CONTROL, 0);
		return controller->wait_for_verb(index).resp;
	}

	void set_pin_control(u8 nid, u8 data) const {
		auto index = controller->submit_verb(cid, nid, cmd::SET_PIN_CONTROL, data);
		controller->wait_for_verb(index);
	}

	[[nodiscard]] u32 get_eapd_enable(u8 nid) const {
		auto index = controller->submit_verb(cid, nid, cmd::GET_EAPD_ENABLE, 0);
		return controller->wait_for_verb(index).resp;
	}

	void set_eapd_enable(u8 nid, u8 data) const {
		auto index = controller->submit_verb(cid, nid, cmd::SET_EAPD_ENABLE, data);
		controller->wait_for_verb(index);
	}

	[[nodiscard]] u8 get_volume_knob(u8 nid) const {
		auto index = controller->submit_verb(cid, nid, cmd::GET_VOLUME_KNOB, 0);
		return controller->wait_for_verb(index).resp;
	}

	void set_volume_knob(u8 nid, u8 data) const {
		auto index = controller->submit_verb(cid, nid, cmd::SET_VOLUME_KNOB, data);
		controller->wait_for_verb(index);
	}

	[[nodiscard]] u32 get_config_default(u8 nid) const {
		auto index = controller->submit_verb(cid, nid, cmd::GET_CONFIG_DEFAULT, 0);
		return controller->wait_for_verb(index).resp;
	}

	[[nodiscard]] u8 get_converter_channel_count(u8 nid) const {
		auto index = controller->submit_verb(cid, nid, cmd::GET_CONVERTER_CHANNEL_COUNT, 0);
		return controller->wait_for_verb(index).resp;
	}

	void set_converter_channel_count(u8 nid, u8 count) const {
		auto index = controller->submit_verb(cid, nid, cmd::SET_CONVERTER_CHANNEL_COUNT, count);
		controller->wait_for_verb(index);
	}

	void set_power_state(u8 nid, u8 data) const {
		auto index = controller->submit_verb(cid, nid, cmd::SET_POWER_STATE, data);
		controller->wait_for_verb(index);
	}

	HdaController* controller;
	kstd::vector<u8> audio_outputs;
	kstd::vector<u8> pin_complexes;
	kstd::vector<HdaWidget> widgets;
	kstd::vector<HdaPath> output_paths;
	u8 cid;
};

static constexpr usize BUFFER_SIZE = CHUNK_SIZE * MAX_DESCRIPTORS;

struct HdaCodecDev : public SoundDevice {
	explicit HdaCodecDev(HdaCodec* codec) : codec {codec} {}

	int get_info(SoundDeviceInfo& res) override {
		res.output_count = codec->output_paths.size();
		return 0;
	}

	int get_output_info(usize index, SoundOutputInfo& res) override {
		if (index >= codec->output_paths.size()) {
			return ERR_INVALID_ARGUMENT;
		}

		auto& path = codec->output_paths[index];
		res.id = &path;
		res.buffer_size = BUFFER_SIZE;

		auto pin = path.nodes[0];

		u8 connectivity = pin->default_config >> 30;
		u8 default_dev = pin->default_config >> 20 & 0b1111;
		u8 connectivity_low = connectivity & 0b1111;
		const char* second_loc;
		if (connectivity_low == 0) {
			second_loc = "n/a";
		}
		else if (connectivity_low == 1) {
			second_loc = "rear";
		}
		else if (connectivity_low == 2) {
			second_loc = "front";
		}
		else if (connectivity_low == 3) {
			second_loc = "left";
		}
		else if (connectivity_low == 4) {
			second_loc = "right";
		}
		else if (connectivity_low == 5) {
			second_loc = "top";
		}
		else if (connectivity_low == 6) {
			second_loc = "bottom";
		}
		else {
			second_loc = "special";
		}

		const char* device = "unknown";
		if (default_dev == 0) {
			device = "line out";
		}
		else if (default_dev == 1) {
			device = "speaker";
		}
		else if (default_dev == 2) {
			device = "hp out";
		}
		else if (default_dev == 3) {
			device = "cd";
		}
		else if (default_dev == 4) {
			device = "spdif out";
		}

		usize name_len = 0;

		auto second_len = strlen(second_loc);
		memcpy(res.name, second_loc, second_len);
		name_len += second_len;
		res.name[name_len++] = ' ';

		auto device_len = strlen(device);
		memcpy(res.name + name_len, device, device_len);
		name_len += device_len;
		res.name[name_len] = 0;

		res.name_len = name_len;

		return 0;
	}

	int set_active_output(void* id) override {
		if (id >= codec->output_paths.begin() && id < codec->output_paths.end()) {
			active_output_path = static_cast<HdaPath*>(id);
			codec->set_active_path(*active_output_path);
			codec->set_volume(*active_output_path, 100);
			return 0;
		}
		else {
			return ERR_INVALID_ARGUMENT;
		}
	}

	int set_output_params(SoundOutputParams& params) override {
		if (!active_output_path) {
			return ERR_UNSUPPORTED;
		}
		PcmFormat fmt {};
		params.sample_rate = fmt.set_sample_rate(params.sample_rate);
		params.channels = fmt.set_channels(params.channels);

		u8 bits;
		switch (params.fmt) {
			case SoundFormatNone:
				bits = 0;
				break;
			case SoundFormatPcmU8:
				bits = 8;
				break;
			case SoundFormatPcmU16:
				bits = 16;
				break;
			case SoundFormatPcmU20:
				bits = 20;
				break;
			case SoundFormatPcmU24:
				bits = 24;
				break;
			case SoundFormatPcmU32:
				bits = 32;
				break;
		}

		u8 real_bits = fmt.set_bits_per_sample(bits);
		if (real_bits != bits) {
			switch (real_bits) {
				case 8:
					params.fmt = SoundFormatPcmU8;
					break;
				case 16:
					params.fmt = SoundFormatPcmU16;
					break;
				case 20:
					params.fmt = SoundFormatPcmU20;
					break;
				case 24:
					params.fmt = SoundFormatPcmU24;
					break;
				case 32:
					params.fmt = SoundFormatPcmU32;
					break;
				default:
					params.fmt = SoundFormatNone;
					break;
			}
		}

		codec->set_format(*active_output_path, fmt);

		auto& stream_space = codec->controller->out_streams[0].space;
		stream_space.store(regs::stream::FMT, fmt.value);

		return 0;
	}

	int set_volume(u8 percentage) override {
		if (!active_output_path) {
			return ERR_UNSUPPORTED;
		}
		if (percentage > 100) {
			percentage = 100;
		}
		codec->set_volume(*active_output_path, percentage);
		return 0;
	}

	int queue_output(const void* buffer, usize& size) override {
		auto& stream = codec->controller->out_streams[0];
		for (usize i = 0; i < size;) {
			auto stream_remaining = stream.remaining_data.load(kstd::memory_order::relaxed);

			usize to_copy;
			if (i + (BUFFER_SIZE - stream_remaining) > size) {
				to_copy = size - i;
			}
			else {
				to_copy = BUFFER_SIZE - stream_remaining;
			}

			for (usize copy_progress = 0; copy_progress < to_copy;) {
				auto desc_index = stream.write_ptr / PAGE_SIZE;
				auto desc_offset = stream.write_ptr % PAGE_SIZE;

				auto remaining = to_copy - copy_progress;
				usize to_copy_desc = kstd::min(remaining, PAGE_SIZE - desc_offset);

				auto& desc = stream.bdl[desc_index];
				auto* ptr = to_virt<void>(desc.address + desc_offset);
				memcpy(ptr, offset(buffer, void*, i + copy_progress), to_copy_desc);

				stream.write_ptr += to_copy_desc;
				if (stream.write_ptr == BUFFER_SIZE) {
					stream.write_ptr = 0;
				}

				copy_progress += to_copy_desc;
			}

			auto old = stream.remaining_data.load(kstd::memory_order::relaxed);
			while (true) {
				if (stream.remaining_data.compare_exchange_weak(
						old,
						old + to_copy,
						kstd::memory_order::relaxed,
						kstd::memory_order::relaxed)) {
					break;
				}
			}

			i += to_copy;
		}

		return 0;
	}

	int play(bool play) override {
		auto& stream_space = codec->controller->out_streams[0].space;
		auto ctl0 = stream_space.load(regs::stream::CTL0);
		if (play) {
			if (!(ctl0 & sdctl0::RUN)) {
				ctl0 |= sdctl0::RUN(true);
				stream_space.store(regs::stream::CTL0, ctl0);
			}
		}
		else {
			if (ctl0 & sdctl0::RUN) {
				ctl0 &= ~sdctl0::RUN;
				stream_space.store(regs::stream::CTL0, ctl0);
			}
		}

		return 0;
	}

	int wait_until_consumed(usize trip_size, usize& remaining) override {
		auto& stream = codec->controller->out_streams[0];

		while (true) {
			auto data_remaining = stream.remaining_data.load(kstd::memory_order::relaxed);
			if (data_remaining <= trip_size) {
				remaining = data_remaining;
				break;
			}

			stream.event.wait();
		}

		return 0;
	}

	HdaCodec* codec {};
	HdaPath* active_output_path {};
};

void HdaController::start() {
	auto gctl = space.load(regs::GCTL);
	if (gctl & gctl::CRST) {
		auto corbctl = space.load(regs::CORBCTL);
		corbctl &= ~corbctl::RUN;
		space.store(regs::CORBCTL, corbctl);
		auto rirbctl = space.load(regs::RIRBCTL);
		rirbctl &= ~rirbctl::DMAEN;
		space.store(regs::RIRBCTL, rirbctl);

		auto gcap = space.load(regs::GCAP);
		auto tmp_in_stream_count = gcap & gcap::ISS;
		auto tmp_out_stream_count = gcap & gcap::OSS;
		for (int i = 0; i < tmp_in_stream_count; ++i) {
			auto stream_space = space.subspace(0x80 + i * 0x20);
			auto ctl0 = stream_space.load(regs::stream::CTL0);
			ctl0 &= ~sdctl0::RUN;
			stream_space.store(regs::stream::CTL0, ctl0);
		}
		for (int i = 0; i < tmp_out_stream_count; ++i) {
			auto stream_space = space.subspace(0x80 + tmp_in_stream_count * 0x20 + i * 0x20);
			auto ctl0 = stream_space.load(regs::stream::CTL0);
			ctl0 &= ~sdctl0::RUN;
			stream_space.store(regs::stream::CTL0, ctl0);
		}
	}

	gctl &= ~gctl::CRST;
	space.store(regs::GCTL, gctl);
	while (space.load(regs::GCTL) & gctl::CRST) {
		udelay(200);
	}
	udelay(200);
	gctl = space.load(regs::GCTL);
	gctl |= gctl::CRST(true);
	space.store(regs::GCTL, gctl);
	while (!(space.load(regs::GCTL) & gctl::CRST));

	auto gcap = space.load(regs::GCAP);
	if (!(gcap & gcap::OK64)) {
		panic("[kernel][hda]: controller doesn't support 64-bit addresses");
	}

	auto corb_size_reg = space.load(regs::CORBSIZE);
	auto corb_cap = corb_size_reg & corbsize::SZCAP;
	u8 chosen_corb_size = 0;
	if (corb_cap & 0b100) {
		chosen_corb_size = 0b10;
		corb_size = 256;
	}
	else if (corb_cap & 0b10) {
		chosen_corb_size = 0b01;
		corb_size = 16;
	}
	else {
		corb_size = 2;
	}
	if (corb_cap != chosen_corb_size << 1) {
		corb_size_reg &= ~corbsize::SIZE;
		corb_size_reg |= corbsize::SIZE(chosen_corb_size);
		space.store(regs::CORBSIZE, corb_size_reg);
	}

	auto rirb_size_reg = space.load(regs::RIRBSIZE);
	auto rirb_cap = rirb_size_reg & rirbsize::SZCAP;
	u8 chosen_rirb_size = 0;
	if (rirb_cap & 0b100) {
		chosen_rirb_size = 0b10;
		rirb_size = 256;
	}
	else if (rirb_cap & 0b10) {
		chosen_rirb_size = 0b01;
		rirb_size = 16;
	}
	else {
		rirb_size = 2;
	}
	if (rirb_cap != chosen_rirb_size << 1) {
		rirb_size_reg &= ~rirbsize::SIZE;
		rirb_size_reg |= rirbsize::SIZE(chosen_rirb_size);
		space.store(regs::RIRBSIZE, rirb_size_reg);
	}

	auto corb_phys = pmalloc(1);
	auto rirb_phys = pmalloc(1);
	auto dma_phys = pmalloc(1);
	assert(corb_phys);
	assert(rirb_phys);
	assert(dma_phys);
	corb = static_cast<VerbDescriptor*>(KERNEL_VSPACE.alloc(PAGE_SIZE));
	rirb = static_cast<ResponseDescriptor*>(KERNEL_VSPACE.alloc(PAGE_SIZE));
	dma_current_pos = static_cast<uint32_t*>(KERNEL_VSPACE.alloc(PAGE_SIZE));
	assert(corb);
	assert(rirb);
	assert(dma_current_pos);
	auto& KERNEL_MAP = KERNEL_PROCESS->page_map;
	assert(KERNEL_MAP.map(reinterpret_cast<u64>(corb), corb_phys, PageFlags::Read | PageFlags::Write, CacheMode::Uncached));
	assert(KERNEL_MAP.map(reinterpret_cast<u64>(rirb), rirb_phys, PageFlags::Read | PageFlags::Write, CacheMode::Uncached));
	assert(KERNEL_MAP.map(reinterpret_cast<u64>(dma_current_pos), dma_phys, PageFlags::Read | PageFlags::Write, CacheMode::Uncached));

	space.store(regs::CORBLBASE, corb_phys);
	space.store(regs::CORBUBASE, corb_phys >> 32);
	space.store(regs::RIRBLBASE, rirb_phys);
	space.store(regs::RIRBUBASE, rirb_phys >> 32);
	space.store(regs::DPLBASE, dplbase::BASE(dma_phys >> 7));
	space.store(regs::DPUBASE, dma_phys >> 32);
	space.store(regs::DPLBASE, space.load(regs::DPLBASE) | dplbase::DPBE(true));

	auto corbctl = space.load(regs::CORBCTL);
	corbctl |= corbctl::RUN(true);
	space.store(regs::CORBCTL, corbctl);
	auto rirbctl = space.load(regs::RIRBCTL);
	rirbctl |= rirbctl::DMAEN(true);
	space.store(regs::RIRBCTL, rirbctl);

	auto rintcnt = space.load(regs::RINTCNT);
	space.store(regs::RINTCNT, (rintcnt & ~0xFF) | 255);

	in_stream_count = gcap & gcap::ISS;
	out_stream_count = gcap & gcap::OSS;
	for (int i = 0; i < in_stream_count; ++i) {
		in_streams[i].space = space.subspace(0x80 + i * 0x20);
	}
	for (int i = 0; i < out_stream_count; ++i) {
		out_streams[i].space = space.subspace(0x80 + in_stream_count * 0x20 + i * 0x20);
		out_streams[i].initialize_output();
	}

	// wait for codec initialization
	mdelay(1);

	auto intctl = space.load(regs::INTCTL);
	intctl |= intctl::GIE(true);
	intctl |= intctl::SIE((1 << (in_stream_count + out_stream_count)) - 1);
	space.store(regs::INTCTL, intctl);

	auto statests = space.load(regs::STATESTS);
	for (int i = 0; i < 15; ++i) {
		if (statests & 1 << i) {
			println("[kernel][hda]: codec found at address ", i);
			auto codec = new HdaCodec {this, static_cast<u8>(i)};

			auto codec_dev = kstd::make_shared<HdaCodecDev>(codec);
			user_dev_add(std::move(codec_dev), CrescentDeviceTypeSound);
		}
	}
}

static InitStatus hda_init(pci::Device& device) {
	println("[kernel][hda]: hda driver initializing for ", Fmt::Hex,
			device.vendor_id, ":", device.device_id, Fmt::Reset);

	auto controller = new HdaController {device};

	println("[kernel][hda]: controller supports ", controller->in_stream_count, " in streams and ", controller->out_stream_count, " out streams");
	println("[kernel][hda]: driver initialized");

	return InitStatus::Success;
}

static InitStatus sst_init(pci::Device& device) {
	println("[kernel]: hda sst init");
	return hda_init(device);
}

static constexpr PciDriver HDA_DRIVER {
	.init = hda_init,
	.match = PciMatch::Class | PciMatch::Subclass,
	._class = 4,
	.subclass = 3
};

PCI_DRIVER(HDA_DRIVER);

static constexpr PciDriver SST_DRIVER {
	.init = sst_init,
	.match = PciMatch::Device,
	.devices {
		{.vendor = 0x8086, .device = 0xA0C8}
	}
};

PCI_DRIVER(SST_DRIVER);
