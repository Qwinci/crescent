#include "arch/x86/mod.hpp"
#include "dev/clock.hpp"
#include "mem/iospace.hpp"
#include "mem/mem.hpp"
#include "mem/vspace.hpp"
#include "sched/process.hpp"
#include "utils/driver.hpp"

namespace regs {
	constexpr BitRegister<u16> GCAP {0};
	constexpr BasicRegister<u8> VMIN {0x2};
	constexpr BasicRegister<u8> VMAJ {0x3};
	constexpr BitRegister<u16> OUTPAY {0x4};
	constexpr BitRegister<u16> INPAY {0x6};
	constexpr BitRegister<u32> GCTL {0x8};
	constexpr BitRegister<u16> WAKEEN {0xC};
	constexpr BitRegister<u16> STATESTS {0xE};
	constexpr BitRegister<u16> GSTS {0x10};
	constexpr BitRegister<u16> OUTSTRMPAY {0x18};
	constexpr BitRegister<u16> INSTRMPAY {0x1A};
	constexpr BitRegister<u32> INTCTL {0x20};
	constexpr BitRegister<u32> INTSTS {0x24};
	constexpr BasicRegister<u32> WALCLK {0x30};
	constexpr BasicRegister<u32> SSYNC {0x38};
	constexpr BasicRegister<u32> CORBLBASE {0x40};
	constexpr BasicRegister<u32> CORBUBASE {0x44};
	constexpr BitRegister<u16> CORBWP {0x48};
	constexpr BitRegister<u16> CORBRP {0x4A};
	constexpr BitRegister<u8> CORBCTL {0x4C};
	constexpr BitRegister<u8> CORBSTS {0x4D};
	constexpr BitRegister<u8> CORBSIZE {0x4E};
	constexpr BasicRegister<u32> RIRBLBASE {0x50};
	constexpr BasicRegister<u32> RIRBUBASE {0x54};
	constexpr BitRegister<u16> RIRBWP {0x58};
	constexpr BasicRegister<u16> RINTCNT {0x5A};
	constexpr BitRegister<u8> RIRBCTL {0x5C};
	constexpr BitRegister<u8> RIRBSTS {0x5D};
	constexpr BitRegister<u8> RIRBSIZE {0x5E};
	constexpr BitRegister<u32> ICOI {0x60};
	constexpr BitRegister<u32> ICII {0x64};
	constexpr BitRegister<u16> ICIS {0x68};
	constexpr BitRegister<u32> DPIBLBASE {0x70};
	constexpr BasicRegister<u32> DPIBUBASE {0x74};

	namespace stream {
		constexpr BitRegister<u8> CTL0 {0};
		constexpr BitRegister<u8> CTL1 {0x1};
		constexpr BitRegister<u8> CTL2 {0x2};
		constexpr BitRegister<u8> STS {0x3};
		constexpr BasicRegister<u32> LPIB {0x4};
		constexpr BasicRegister<u32> CBL {0x8};
		constexpr BasicRegister<u16> LVI {0xC};
		constexpr BasicRegister<u16> FIFOS {0x10};
		constexpr BitRegister<u16> FMT {0x12};
		constexpr BasicRegister<u32> BDPL {0x18};
		constexpr BasicRegister<u32> BDPU {0x1C};
	}
}

namespace gcap {
	constexpr BitField<u16, bool> OK64 {0, 1};
	constexpr BitField<u16, u8> NSDO {1, 2};
	constexpr BitField<u16, u8> BSS {3, 5};
	constexpr BitField<u16, u8> ISS {8, 4};
	constexpr BitField<u16, u8> OSS {12, 4};
}

namespace gctl {
	constexpr BitField<u32, bool> CRST {0, 1};
	constexpr BitField<u32, bool> FCNTRL {1, 1};
	constexpr BitField<u32, bool> UNSOL {8, 1};
}

namespace intctl {
	constexpr BitField<u32, u32> SIE {0, 30};
	constexpr BitField<u32, bool> CIE {30, 1};
	constexpr BitField<u32, bool> GIE {31, 1};
}

namespace intsts {
	constexpr BitField<u32, u32> SIS {0, 30};
	constexpr BitField<u32, bool> CIS {30, 1};
	constexpr BitField<u32, bool> GIS {31, 1};
}

namespace corbwp {
	constexpr BitField<u16, u8> WP {0, 8};
}

namespace corbrp {
	constexpr BitField<u16, u8> RP {0, 8};
	constexpr BitField<u16, bool> RST {15, 1};
}

namespace corbctl {
	constexpr BitField<u8, bool> MEIE {0, 1};
	constexpr BitField<u8, bool> RUN {1, 1};
}

namespace corbsize {
	constexpr BitField<u8, u8> SIZE {0, 2};
	constexpr BitField<u8, u8> SZCAP {4, 4};
}

namespace rirbwp {
	constexpr BitField<u16, u8> WP {0, 8};
	constexpr BitField<u16, bool> RST {15, 1};
}

namespace rirbctl {
	constexpr BitField<u8, bool> INTCTL {0, 1};
	constexpr BitField<u8, bool> DMAEN {1, 1};
	constexpr BitField<u8, bool> OIC {2, 1};
}

namespace rirbsts {
	constexpr BitField<u8, bool> INTFL {0, 1};
	constexpr BitField<u8, bool> bois {2, 1};
}

namespace rirbsize {
	constexpr BitField<u8, u8> SIZE {0, 2};
	constexpr BitField<u8, u8> SZCAP {4, 4};
}

namespace dplbase {
	constexpr BitField<u32, bool> DPBE {0, 1};
	constexpr BitField<u32, u32> BASE {7, 25};
}

namespace sdctl0 {
	constexpr BitField<u8, bool> RST {0, 1};
	constexpr BitField<u8, bool> RUN {1, 1};
	constexpr BitField<u8, bool> IOCE {2, 1};
	constexpr BitField<u8, bool> FEIE {3, 1};
	constexpr BitField<u8, bool> DEIE {4, 1};
}

namespace sdctl2 {
	constexpr BitField<u8, u8> STRIPE {0, 2};
	constexpr BitField<u8, bool> TP {2, 1};
	constexpr BitField<u8, bool> DIR {3, 1};
	constexpr BitField<u8, u8> STRM {4, 4};
}

namespace sdlvi {
	constexpr BitField<u16, u8> LVI {0, 8};
}

namespace sdsts {
	constexpr BitField<u8, bool> BCIS {2, 1};
	constexpr BitField<u8, bool> FIFOE {3, 1};
	constexpr BitField<u8, bool> DESE {4, 1};
	constexpr BitField<u8, bool> FIFORDY {5, 1};
}

namespace sdfmt [[gnu::visibility("hidden")]] {
	constexpr BitField<u16, u8> CHAN {0, 4};
	constexpr BitField<u16, u8> BITS {4, 3};
	constexpr BitField<u16, u8> DIV {8, 3};
	constexpr BitField<u16, u8> MULT {11, 3};
	constexpr BitField<u16, u8> BASE {14, 1};

	constexpr u8 BASE_48KHZ = 0;
	constexpr u8 BASE_441KHZ = 1;

	constexpr u8 MULT_1 = 0b000;
	constexpr u8 MULT_2 = 0b001;
	constexpr u8 MULT_3 = 0b010;
	constexpr u8 MULT_4 = 0b011;

	constexpr u8 DIV_1 = 0b000;
	constexpr u8 DIV_2 = 0b001;
	constexpr u8 DIV_3 = 0b010;
	constexpr u8 DIV_4 = 0b011;
	constexpr u8 DIV_5 = 0b100;
	constexpr u8 DIV_6 = 0b101;
	constexpr u8 DIV_7 = 0b110;
	constexpr u8 DIV_8 = 0b111;

	constexpr u8 BITS_8 = 0b000;
	constexpr u8 BITS_16 = 0b001;
	constexpr u8 BITS_20 = 0b010;
	constexpr u8 BITS_24 = 0b011;
	constexpr u8 BITS_32 = 0b100;
}

struct BufferDescriptor {
	u64 address;
	u32 length;
	u32 ioc;
};

namespace verb {
	constexpr BitField<u32, u32> PAYLOAD {0, 20};
	constexpr BitField<u32, u8> NODE_ID {20, 8};
	constexpr BitField<u32, u8> CODEC_ADDRESS {28, 4};
}

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

namespace pcm_format {
	using namespace sdfmt;
	constexpr BitField<u16, bool> PCM {15, 1};
}

struct PcmFormat {
	BitValue<u16> value;
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

struct StreamChannelPair {
	u8 channel;
	u8 stream;
};

struct Controller {
	constexpr explicit Controller(pci::Device& device) {
		auto* virt = device.map_bar(0);
		assert(virt);
		space = IoSpace {virt};

		device.enable_mem_space(true);
		device.enable_bus_master(true);

		if (auto power = static_cast<pci::caps::Power*>(device.hdr0->get_cap(pci::Cap::Power, 0))) {
			power->set_state(pci::PowerState::D3Hot);
			mdelay(1);
			power->set_state(pci::PowerState::D0);
		}

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

	IoSpace space;
	IoSpace in_streams[16] {};
	IoSpace out_streams[16] {};
	VerbDescriptor* corb {};
	ResponseDescriptor* rirb {};
	u16 corb_size {};
	u16 rirb_size {};
	u8 in_stream_count {};
	u8 out_stream_count {};
};

struct Codec;

struct Widget {
	Codec* codec;
	kstd::vector<u8> connections;
	u32 in_amp_caps;
	u32 out_amp_caps;
	u32 pin_caps;
	u32 default_config;
	u8 nid;
};

struct Codec {
	constexpr Codec(Controller* controller, u8 cid) : controller {controller}, cid {cid} {
		enumerate();
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
					for (u8 j = 0; j < kstd::max(conn_list_len - i, 4); ++j) {
						u8 nid = resp >> (j * 8) & 0xFF;
						connections.push(nid);
					}
				}

				Widget widget {
					.codec = this,
					.connections {std::move(connections)},
					.in_amp_caps = in_amp_caps,
					.out_amp_caps = out_amp_caps,
					.pin_caps = pin_caps,
					.default_config = default_config,
					.nid = widget_i
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

		auto& pin = widgets[3];
		set_selected_connection(pin.nid, 0);
		if (pin.pin_caps & 1 << 16) {
			set_eapd_enable(pin.nid, 1 << 1);
		}
		u8 gain = pin.out_amp_caps >> 8 & 0x7F;
		// set output amp, set left amp, set right amp and gain
		u16 amp_data = 1 << 15 | 1 << 13 | 1 << 12 | gain;
		set_amp_gain_mute(pin.nid, amp_data);
		set_power_state(pin.nid, 0);

		/*auto& mixer = widgets[0xC];
		set_selected_connection(mixer.nid, 0);
		auto in_amp_data = 1 << 14 | 1 << 13 | 1 << 12 | (mixer.in_amp_caps & 0x7F);
		set_amp_gain_mute(mixer.nid, in_amp_data);
		set_power_state(mixer.nid, 0);*/

		auto& output = widgets[2];
		amp_data &= ~0x7F;
		amp_data |= output.out_amp_caps >> 8 & 0x7F;
		set_amp_gain_mute(output.nid, amp_data);
		set_converter_control(output.nid, {
			.channel = 0,
			.stream = 1
		});
		set_power_state(output.nid, 0);

		PcmFormat fmt {};
		fmt.value |= pcm_format::PCM(true);
		fmt.value |= pcm_format::BASE(sdfmt::BASE_441KHZ);
		fmt.value |= pcm_format::MULT(sdfmt::MULT_1);
		fmt.value |= pcm_format::DIV_1;
		fmt.value |= pcm_format::BITS(sdfmt::BITS_16);
		// 2 channels
		fmt.value |= pcm_format::CHAN(1);
		set_converter_format(output.nid, fmt);
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

	Controller* controller;
	kstd::vector<u8> audio_outputs;
	kstd::vector<u8> pin_complexes;
	kstd::vector<Widget> widgets;
	u8 cid;
};

void Controller::start() {
	auto gctl = space.load(regs::GCTL);
	gctl &= ~gctl::CRST;
	space.store(regs::GCTL, gctl);
	udelay(200);
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
	assert(corb_phys);
	assert(rirb_phys);
	corb = static_cast<VerbDescriptor*>(KERNEL_VSPACE.alloc(PAGE_SIZE));
	rirb = static_cast<ResponseDescriptor*>(KERNEL_VSPACE.alloc(PAGE_SIZE));
	assert(corb);
	assert(rirb);
	auto& KERNEL_MAP = KERNEL_PROCESS->page_map;
	assert(KERNEL_MAP.map(reinterpret_cast<u64>(corb), corb_phys, PageFlags::Read | PageFlags::Write, CacheMode::Uncached));
	assert(KERNEL_MAP.map(reinterpret_cast<u64>(rirb), rirb_phys, PageFlags::Read | PageFlags::Write, CacheMode::Uncached));

	space.store(regs::CORBLBASE, corb_phys);
	space.store(regs::CORBUBASE, corb_phys >> 32);
	space.store(regs::RIRBLBASE, rirb_phys);
	space.store(regs::RIRBUBASE, rirb_phys >> 32);

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
		in_streams[i] = space.subspace(0x80 + i * 0x20);
	}
	for (int i = 0; i < out_stream_count; ++i) {
		out_streams[i] = space.subspace(0x80 + in_stream_count * 0x20 + i * 0x20);
	}

	// wait for codec initialization
	mdelay(1);

	auto statests = space.load(regs::STATESTS);
	for (int i = 0; i < 15; ++i) {
		if (statests & 1 << i) {
			println("[kernel][hda]: codec found at address ", i);
			new Codec {this, static_cast<u8>(i)};
		}
	}
}

static bool hda_init(pci::Device& device) {
	println("[kernel][hda]: hda driver initializing for ", Fmt::Hex,
			device.hdr0->common.vendor_id, ":", device.hdr0->common.device_id, Fmt::Reset);

	void* output_data;
	usize output_size;

	Module module {};
	x86_get_module(module, "output.raw");
	output_data = module.data;
	output_size = module.size;

	auto controller = new Controller {device};

	println("[kernel][hda]: controller support ", controller->out_stream_count, " out streams");
	assert(controller->out_stream_count > 1);
	auto stream_space = controller->out_streams[1];

	auto ctl2 = stream_space.load(regs::stream::CTL2);
	ctl2 &= ~sdctl2::STRM;
	ctl2 |= sdctl2::STRM(1);
	stream_space.store(regs::stream::CTL2, ctl2);

	usize max_size = 1024_usize * 1024 * 1024 * 4;
	assert(output_size < max_size);

	// cyclic buffer length in bytes
	stream_space.store(regs::stream::CBL, output_size * 2);

	// last valid index in buffer descriptor list
	auto lvi = stream_space.load(regs::stream::LVI);
	lvi &= ~sdlvi::LVI;
	// two entries
	lvi |= sdlvi::LVI(1);
	stream_space.store(regs::stream::LVI, lvi);

	auto fmt = stream_space.load(regs::stream::FMT);
	fmt &= ~sdfmt::BASE;
	fmt |= sdfmt::BASE(sdfmt::BASE_441KHZ);
	fmt &= ~sdfmt::MULT;
	fmt |= sdfmt::MULT(sdfmt::MULT_1);
	fmt &= ~sdfmt::DIV;
	fmt |= sdfmt::DIV_1;
	fmt &= ~sdfmt::BITS;
	fmt |= sdfmt::BITS(sdfmt::BITS_16);
	fmt &= ~sdfmt::CHAN;
	// 2 channels
	fmt |= sdfmt::CHAN(1);
	stream_space.store(regs::stream::FMT, fmt);

	auto& KERNEL_MAP = KERNEL_PROCESS->page_map;

	auto bdl_phys = pmalloc(1);
	assert(bdl_phys);
	auto* bdl = static_cast<BufferDescriptor*>(KERNEL_VSPACE.alloc(PAGE_SIZE));
	assert(bdl);
	assert(KERNEL_MAP.map(reinterpret_cast<u64>(bdl), bdl_phys, PageFlags::Read | PageFlags::Write, CacheMode::Uncached));

	stream_space.store(regs::stream::BDPL, bdl_phys);
	stream_space.store(regs::stream::BDPU, bdl_phys >> 32);

	auto output_phys = to_phys(output_data);
	bdl[0] = {
		.address = output_phys,
		.length = static_cast<u32>(output_size),
		.ioc = 0
	};
	bdl[1] = {
		.address = output_phys,
		.length = static_cast<u32>(output_size),
		.ioc = 0
	};

	auto ctl0 = stream_space.load(regs::stream::CTL0);
	ctl0 |= sdctl0::RUN(true);
	stream_space.store(regs::stream::CTL0, ctl0);

	println("[kernel][hda]: driver initialized");

	return true;
}

static PciDriver HDA_DRIVER {
	.init = hda_init,
	.match = PciMatch::Class | PciMatch::Subclass,
	._class = 4,
	.subclass = 3
};

PCI_DRIVER(HDA_DRIVER);
