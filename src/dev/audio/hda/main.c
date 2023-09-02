#include "dev/dev.h"
#include "mem/utils.h"
#include "dev/timer.h"
#include "mem/pmalloc.h"
#include "mem/allocator.h"
#include "string.h"
#include "arch/mod.h"
#include "arch/interrupts.h"
#include "sys/dev.h"
#include "dev/audio/snd_dev.h"
#include "utils/math.h"

typedef struct {
	u8 ctl[3];
	u8 sts;
	u32 lpib;
	u32 cbl;
	u16 lvi;
	u16 reserved9;
	u16 fifod;
	u16 fmt;
	u32 reserved10;
	u32 bdpl;
	u32 bdpu;
} StreamDesc;

typedef struct {
	u16 gcap;
	u8 vmin;
	u8 vmaj;
	u16 outpay;
	u16 inpay;
	u32 gctl;
	u16 wakeen;
	u16 wakests;
	u16 gsts;
	u8 reserved1[6];
	u16 outstrmpay;
	u16 instrmpay;
	u8 reserved2[4];
	u32 intctl;
	u32 intsts;
	u8 reserved3[8];
	u32 walclk;
	u32 reserved4;
	u32 ssync;
	u32 reserved5;
	u32 corblbase;
	u32 corbubase;
	u16 corbwp;
	u16 corbrp;
	u8 corbctl;
	u8 corbsts;
	u8 corbsize;
	u8 reserved6;
	u32 rirblbase;
	u32 rirbubase;
	u16 rirbwp;
	u16 rintcnt;
	u8 rirbctl;
	u8 rirbsts;
	u8 rirbsize;
	u8 reserved7;
	u32 icoi;
	u32 icii;
	u16 icis;
	u8 reserved8[6];
	u32 dpiblbase;
	u32 dpibubase;
	u8 reserved9[8];
	StreamDesc isd0;
} Registers;

#define GCAP_64OK(value) ((value) & 1)
#define GCAP_NSDO(value) ((value) >> 1 & 0b11)
#define GCAP_BSS(value) ((value) >> 3 & 0b11111)
#define GCAP_ISS(value) ((value) >> 8 & 0b1111)
#define GCAP_OSS(value) ((value) >> 12 & 0b1111)

#define GCTL_CRST(value) ((value) & 1)
#define GCTL_CRST_V (1)
#define GCTL_FCNTRL(value) ((value) >> 1 & 1)
#define GCTL_FCNTRL_V (1 << 1)
#define GCTL_UNSOL(value) ((value) >> 8 & 1)
#define GCTL_UNSOL_V (1 << 8)

#define CORBCTL_CORBRUN(value) ((value) >> 1 & 1)
#define CORBCTL_CORBRUN_V(value) ((value) << 1)

#define CORBSIZE_CAP(value) ((value) >> 4 & 0b1111)
#define CORBSIZE_SIZE(value) ((value) & 0b11)
#define CORBSIZE_SIZE_V(value) ((value) & 0b11)

#define CORBRP_CORBRPRST(value) ((value) >> 15)
#define CORBRP_CORBRPRST_V (1 << 15)
#define CORBRP_CORBRP(value) ((value) & 0xFF)
#define CORBRP_CORBRP_V(value) ((value) & 0xFF)

typedef struct {
	u32 response;
	u32 response_ex;
} RirbEntry;

#define CORB_ENTRY_DATA(data) ((data))
#define CORB_ENTRY_CMD(cmd) ((cmd) << 8)
#define CORB_ENTRY_NID(nid) ((nid) << 20)
#define CORB_ENTRY_CID(cid) ((cid) << 28)

#define RESP_EX_CODEC(value) ((value) & 0b1111)
#define RESP_EX_UNSOL(value) ((value) >> 4 & 1)

typedef struct Widget {
	struct Widget* next;
	u8* con_list;
	u32 pin_caps;
	u32 in_amp;
	u32 out_amp;
	u32 vol_caps;
	u32 audio_caps;
	u32 conf;
	u8 con_list_len;
	u8 nid;
	u8 cid;
	u8 type;
} Widget;

typedef struct {
	Widget** data;
	usize len;
} WidgetNidTab;

static void widget_nid_tab_insert(WidgetNidTab* self, u8 nid, Widget* widget) {
	if (nid >= self->len) {
		usize new_len = MAX(self->len * 2, nid + 1);
		self->data = krealloc(self->data, self->len * sizeof(Widget*), new_len * sizeof(Widget*));
		assert(self->data);
		self->len = new_len;
	}
	self->data[nid] = widget;
}

typedef struct {
	Widget** widgets;
	usize len;
} SignalPath;

static void signal_path_add_widget(SignalPath* self, Widget* widget) {
	self->widgets = krealloc(self->widgets, self->len * sizeof(Widget*), (self->len + 1) * sizeof(Widget*));
	assert(self->widgets);
	self->widgets[self->len++] = widget;
}

typedef struct {
	SndDev snd_dev;
	PciDev* pci_dev;
	volatile Registers* regs;
	u32* corb;
	volatile RirbEntry* rirb;
	usize input_stream_count;
	StreamDesc* input_streams;
	usize output_stream_count;
	StreamDesc* output_streams;
	IrqHandler irq_handler;

	Widget* outputs;
	Widget* inputs;
	Widget* mixers;
	Widget* selectors;
	Widget* pins;
	Widget* powers;
	Widget* volume_knobs;
	Widget* beep_generators;
	WidgetNidTab nid_tables[16];
	SignalPath* signal_paths;
	usize signal_paths_len;
	SignalPath* tmp_signal_path;

	bool used_input_streams[16];
	bool used_output_streams[16];
	AudioStream output_stream_infos[16];
} Controller;

static SignalPath* controller_add_signal_path(Controller* self) {
	usize new_len = self->signal_paths_len + 1;
	self->signal_paths = (SignalPath*) krealloc(
		self->signal_paths,
		self->signal_paths_len * sizeof(SignalPath),
		new_len * sizeof(SignalPath));
	assert(self->signal_paths);
	self->signal_paths_len = new_len;
	memset(&self->signal_paths[new_len - 1], 0, sizeof(SignalPath));
	return &self->signal_paths[new_len - 1];
}

static RirbEntry hda_send_cmd_await(Controller* self, u8 data, u16 cmd, u8 nid, u8 cid) {
	u8 idx = (self->regs->corbwp & 0xFF) + 1;
	u32* entry = &self->corb[idx];
	*entry = CORB_ENTRY_DATA(data) | CORB_ENTRY_CMD(cmd) | CORB_ENTRY_NID(nid) | CORB_ENTRY_CID(cid);

	self->regs->corbwp = (self->regs->corbwp & 0xFF00) | idx;

	while (CORBRP_CORBRP(self->regs->corbrp) != idx);

	while ((self->regs->rirbwp & 0xFF) != idx);

	return self->rirb[idx];
}

#define CMD_SET_CONVERTER_FORMAT 0x2
#define CMD_SET_AMP_GAIN_MUTE 0x3
#define CMD_SET_SELECTED_CONNECTION 0x701
#define CMD_SET_EAPD 0x70C
#define CMD_GET_PARAMETER 0xF00
#define CMD_GET_CONNECTION_LIST_ENTRY 0xF02
#define CMD_SET_POWER_STATE 0x705
#define CMD_SET_CONVERTER_CHANNEL 0x706
#define CMD_SET_PIN_CONTROL 0x707
#define CMD_GET_CONF_DEFAULT 0xF1C

#define PARAM_DEV_ID 0
#define PARAM_NODE_CNT 4
#define PARAM_FUNC_GROUP_TYPE 5
#define PARAM_AUDIO_WIDGET_CAPABILITIES 9
#define PARAM_SUPPORTED_PCM_RATES 0xA
#define PARAM_SUPPORTED_FORMATS 0xB
#define PARAM_PIN_CAPABILITIES 0xC
#define PARAM_INPUT_AMP_CAPABILITIES 0xD
#define PARAM_OUTPUT_AMP_CAPABILITIES 0x12
#define PARAM_CONNECTION_LIST_LEN 0xE
#define PARAM_VOLUME_CAPABILITIES 0x13

#define FUNC_GROUP_AUDIO 1

#define WIDGET_AUDIO_OUT 0
#define WIDGET_AUDIO_IN 1
#define WIDGET_AUDIO_MIXER 2
#define WIDGET_AUDIO_SELECTOR 3
#define WIDGET_AUDIO_PIN_COMPLEX 4
#define WIDGET_POWER 5
#define WIDGET_VOLUME 6
#define WIDGET_BEEP_GENERATOR 7

typedef struct {
	u64 addr;
	u32 len;
	u32 ioc;
} BufferDesc;

static void find_output_paths(Controller* self) {
	for (Widget* pin = self->pins; pin; pin = pin->next) {
		// check if output-capable
		if (!(pin->pin_caps & 1 << 4)) {
			continue;
		}
		u8 connectivity = pin->conf >> 30;
		// no physical connection
		if (connectivity == 1) {
			continue;
		}

		typedef struct {
			Widget* widget;
			u8 con_i;
			u8 con_range_i;
			u8 con_range_end;
		} StackEntry;

		StackEntry stack_mem[20];
		StackEntry* stack_ptr = stack_mem;
		*stack_ptr++ = (StackEntry) {.widget = pin, .con_i = 0, .con_range_i = 0xFF};

		while (true) {
			if (stack_ptr == stack_mem) {
				break;
			}

			StackEntry* cur_entry = &stack_ptr[-1];
			Widget* cur_widget = cur_entry->widget;
			if (cur_entry->con_i >= cur_widget->con_list_len) {
				--stack_ptr;
				continue;
			}

			if (cur_entry->con_range_i > cur_entry->con_range_end) {
				cur_entry->con_range_i = cur_widget->con_list[cur_entry->con_i++];
				// the first connection can't be a range
				assert(cur_entry->con_range_i >> 7 == 0);
				if (cur_entry->con_i < cur_widget->con_list_len && cur_widget->con_list[cur_entry->con_i] & 1 << 7) {
					cur_entry->con_range_end = cur_widget->con_list[cur_entry->con_i++] & 0x7F;
				}
				else {
					cur_entry->con_range_end = cur_entry->con_range_i;
				}
			}

			u8 nid = cur_entry->con_range_i++;
			assert(nid < self->nid_tables[pin->cid].len);
			assert(pin->cid < 16);
			assert(nid < self->nid_tables[pin->cid].len);
			Widget* assoc_widget = self->nid_tables[pin->cid].data[nid];
			// input-capable pin or an output
			if ((assoc_widget->type == WIDGET_AUDIO_PIN_COMPLEX && assoc_widget->pin_caps & 1 << 5) ||
				assoc_widget->type == WIDGET_AUDIO_OUT) {
				SignalPath* signal_path = controller_add_signal_path(self);
				for (StackEntry* entry = stack_mem; entry < stack_ptr; ++entry) {
					signal_path_add_widget(signal_path, entry->widget);
				}
				signal_path_add_widget(signal_path, assoc_widget);
			}
			else {
				bool circular_path = false;
				for (StackEntry* entry = stack_mem; entry < stack_ptr; ++entry) {
					if (entry->widget == assoc_widget) {
						circular_path = true;
						break;
					}
				}
				// don't save paths which are circular or over 20 widgets long
				if (circular_path || stack_ptr >= stack_mem + 20) {
					continue;
				}
				*stack_ptr++ = (StackEntry) {.widget = assoc_widget, .con_i = 0, .con_range_i = 0xFF};
			}
		}
	}
}

static void codec_enumerate(Controller* self, u8 num) {
	kprintf("codec at address %u\n", num);

	RirbEntry res = hda_send_cmd_await(self, PARAM_NODE_CNT, CMD_GET_PARAMETER, 0, num);

	u8 num_of_func_group_nodes = res.response & 0xFF;
	u8 starting_func_group_node = res.response >> 16 & 0xFF;

	for (u8 i = 0; i < num_of_func_group_nodes; ++i) {
		u8 group_nid = starting_func_group_node + i;
		res = hda_send_cmd_await(self, PARAM_FUNC_GROUP_TYPE, CMD_GET_PARAMETER, group_nid, num);
		bool group_unsol_capable = res.response >> 8 & 1;
		u8 group_type = res.response & 0xFF;

		if (group_type != FUNC_GROUP_AUDIO) {
			continue;
		}

		kprintf("found audio function group at nid %u\n", group_nid);

		res = hda_send_cmd_await(self, PARAM_NODE_CNT, CMD_GET_PARAMETER, group_nid, num);
		u8 num_of_widget_nodes = res.response & 0xFF;
		u8 starting_widget_node = res.response >> 16 & 0xFF;

		kprintf("%u widgets\n", num_of_widget_nodes);

		for (u8 widget_i = 0; widget_i < num_of_widget_nodes; ++widget_i) {
			u8 nid = starting_widget_node + widget_i;
			res = hda_send_cmd_await(self, PARAM_AUDIO_WIDGET_CAPABILITIES, CMD_GET_PARAMETER, nid, num);

			u8 type = res.response >> 20 & 0xF;
			//kprintf("nid %u type %X\n", nid, type);

			Widget* widget = kmalloc(sizeof(Widget));
			assert(widget);

			widget->audio_caps = res.response;
			widget->pin_caps = hda_send_cmd_await(self, PARAM_PIN_CAPABILITIES, CMD_GET_PARAMETER, nid, num).response;
			widget->in_amp = hda_send_cmd_await(self, PARAM_INPUT_AMP_CAPABILITIES, CMD_GET_PARAMETER, nid, num).response;
			widget->out_amp = hda_send_cmd_await(self, PARAM_OUTPUT_AMP_CAPABILITIES, CMD_GET_PARAMETER, nid, num).response;
			widget->con_list_len = hda_send_cmd_await(self, PARAM_CONNECTION_LIST_LEN, CMD_GET_PARAMETER, nid, num).response;
			widget->vol_caps = hda_send_cmd_await(self, PARAM_VOLUME_CAPABILITIES, CMD_GET_PARAMETER, nid, num).response;
			widget->conf = hda_send_cmd_await(self, 0, CMD_GET_CONF_DEFAULT, nid, num).response;

			assert(widget->con_list_len >> 7 == 0 && "long nids are not implemented");

			widget->con_list = kmalloc(widget->con_list_len);
			assert(!widget->con_list_len || widget->con_list);
			for (u8 j = 0; j < widget->con_list_len; j += 4) {
				u32 con_res = hda_send_cmd_await(self, j, CMD_GET_CONNECTION_LIST_ENTRY, nid, num).response;
				if (!con_res) {
					kfree(widget->con_list, widget->con_list_len);
					widget->con_list = NULL;
					widget->con_list_len = 0;
					break;
				}
				for (u8 short_i = 0; short_i < (u8) MIN(widget->con_list_len - j, 4); ++short_i) {
					widget->con_list[j + short_i] = con_res >> (short_i * 8) & 0xFF;
				}
			}

			widget->nid = nid;
			widget->cid = num;
			widget->type = type;
			widget->next = NULL;

			if (type == WIDGET_AUDIO_OUT) {
				widget->next = self->outputs;
				self->outputs = widget;
			}
			else if (type == WIDGET_AUDIO_IN) {
				widget->next = self->inputs;
				self->inputs = widget;
			}
			else if (type == WIDGET_AUDIO_MIXER) {
				widget->next = self->mixers;
				self->mixers = widget;
			}
			else if (type == WIDGET_AUDIO_SELECTOR) {
				widget->next = self->selectors;
				self->selectors = widget;
			}
			else if (type == WIDGET_AUDIO_PIN_COMPLEX) {
				widget->next = self->pins;
				self->pins = widget;
			}
			else if (type == WIDGET_POWER) {
				widget->next = self->powers;
				self->powers = widget;
			}
			else if (type == WIDGET_BEEP_GENERATOR) {
				widget->next = self->beep_generators;
				self->beep_generators = widget;
			}

			widget_nid_tab_insert(&self->nid_tables[num], nid, widget);
		}

		/*u16 pin_connected_to_nid = self->pins->con_list[0] & 0x7FFF;
		bool range_nids = self->pins->con_list[0] >> 15;
		assert(!range_nids);
		assert(self->outputs->nid == pin_connected_to_nid);

		u32 out_amp_res = hda_send_cmd_await(self, PARAM_OUTPUT_AMP_CAPABILITIES, CMD_GET_PARAMETER, self->outputs->nid, num).response;
		u32 pin_amp_res = hda_send_cmd_await(self, PARAM_OUTPUT_AMP_CAPABILITIES, CMD_GET_PARAMETER, self->pins->nid, num).response;

		u8 out_offset = out_amp_res & 0x7F;
		u8 out_num_steps = out_amp_res >> 8 & 0x7F;
		u8 out_step_size = out_amp_res >> 16 & 0x7F;

		// set_output_amp | set_input_amp | set_left_amp | set_right_amp | 7bit_gain
		u16 data0 = 1 << 15 | 1 << 14 | 1 << 13 | 1 << 12 | (out_offset);

		u8 data_low = data0 & 0xFF;
		u16 cmd = CMD_SET_AMP_GAIN_MUTE << 8 | (data0 >> 8);

		hda_send_cmd_await(self, data_low, cmd, self->pins->nid, num);
		hda_send_cmd_await(self, data_low, cmd, self->outputs->nid, num);

		// 4 stream num (4bit)
		self->output_streams->ctl[2] |= 1 << 4;

		Module song_mod = arch_get_module("output_raw");
		assert(song_mod.base);

		// cyclic buffer size
		self->output_streams->cbl = song_mod.size * 2;

		// last valid index in the buffer descriptor list (1 == 2 entries) 8bit
		self->output_streams->lvi |= 1;

		// sample_values 0 == 8, 1 == 16, 2 == 20, 3 == 24, 4 == 32
		// sample_rates 0 == 48khz/44.1khz, 1 == 24khz/22.05khz, 2 == 16khz/32khz, 3 == 11.025khz, 4 == 9.6khz,
		// 5 == 9khz, 6 == divide_by_7, 7 == 6khz
		// 2_channels | 16bit_samples | sample_rate_48khz | sample_rate_multiple_48khz_less | sample_base_rate_44khz
		self->output_streams->fmt |= (1) | (1 << 4) | (0 << 8) | (0 << 11) | (1 << 14);

		Page* buffer_desc_list_page = pmalloc(1);
		assert(buffer_desc_list_page);

		self->output_streams->bdpl = buffer_desc_list_page->phys;
		self->output_streams->bdpu = buffer_desc_list_page->phys >> 32;

		BufferDesc* buffer_desc_list = (BufferDesc*) to_virt(buffer_desc_list_page->phys);
		memset(buffer_desc_list, 0, PAGE_SIZE);

		// pcm16 44khz
		u16 data1 = (1) | (1 << 4) | (0 << 8) | (0 << 11) | (1 << 14) | (0 << 15);

		u8 data1_low = data1 & 0xFF;
		u16 cmd1 = CMD_SET_CONVERTER_FORMAT << 8 | (data1 >> 8);

		hda_send_cmd_await(self, data1_low, cmd1, self->outputs->nid, num);

		// 4bit_channel | 4bit_stream
		u8 channel_data = 0 | (1 << 4);
		hda_send_cmd_await(self, channel_data, CMD_SET_CONVERTER_CHANNEL, self->outputs->nid, num);

		// out_enable
		u8 pin_control = 1 << 6;
		hda_send_cmd_await(self, pin_control, CMD_SET_PIN_CONTROL, self->pins->nid, num);

		// enable external amp
		hda_send_cmd_await(self, 1 << 0, CMD_SET_EAPD, self->outputs->nid, num);

		// power output on (D0)
		hda_send_cmd_await(self, 0, CMD_SET_POWER_STATE, self->outputs->nid, num);

		usize page_count = ALIGNUP(song_mod.size, PAGE_SIZE) / PAGE_SIZE;

		Page* pages = pmalloc(page_count);
		assert(pages);

		buffer_desc_list[0].addr = pages->phys;
		buffer_desc_list[0].len = song_mod.size;
		buffer_desc_list[0].ioc = 0;
		buffer_desc_list[1].addr = pages->phys;
		buffer_desc_list[1].len = song_mod.size;
		buffer_desc_list[1].ioc = 0;

		void* buffer = to_virt(pages->phys);
		memcpy(buffer, song_mod.base, song_mod.size);

		// enable stream 0
		self->output_streams->ctl[0] |= 1 << 1;*/
	}
}

static IrqStatus irq_handler(void*, void* void_self) {
	Controller* self = (Controller*) void_self;

	u32 status = self->regs->intsts;
	if (!status) {
		return IRQ_NACK;
	}

	usize start = self->input_stream_count ? self->input_stream_count - 1 : 0;
	for (usize i = 0; i < self->output_stream_count; ++i) {
		if (status & 1 << (start + i)) {
			StreamDesc* stream = &self->output_streams[i - 1];
			AudioStream* audio_stream = &self->output_stream_infos[i - 1];
			if (self->snd_dev.user_callback) {
				if (audio_stream->write_to_end) {
					self->snd_dev.user_callback(
						offset(audio_stream->buffer, void*, audio_stream->single_buffer_size),
						audio_stream->single_buffer_size, self->snd_dev.user_data);
					audio_stream->write_to_end = false;
				}
				else {
					self->snd_dev.user_callback(audio_stream->buffer, audio_stream->single_buffer_size, self->snd_dev.user_data);
					audio_stream->write_to_end = true;
				}
			}
		}
	}

	//kprintf("hda irq\n");

	return IRQ_ACK;
}

static AudioStream* snd_create_stream(SndDev* snd_self, AudioParams* params) {
	if (!params->sample_rate || !params->format || !params->channels) {
		return NULL;
	}
	if (params->preferred_buffer_size == 0) {
		u8 sample_size;
		switch (params->format) {
			case SND_PCM_8:
				sample_size = 1;
				break;
			case SND_PCM_16NE:
				sample_size = 2;
				break;
			case SND_PCM_INVALID:
				__builtin_unreachable();
		}

		params->preferred_buffer_size = params->sample_rate * params->channels * sample_size;
	}
	params->preferred_buffer_size = ALIGNUP(params->preferred_buffer_size, PAGE_SIZE);
	params->preferred_buffer_size *= 2;
	if (params->preferred_buffer_size > PAGE_SIZE * 256) {
		params->preferred_buffer_size = PAGE_SIZE * 256;
	}

	void* buffer = kmalloc(params->preferred_buffer_size);
	if (!buffer) {
		return NULL;
	}

	Controller* self = container_of(snd_self, Controller, snd_dev);
	u8 id = 0xFF;
	for (usize i = 0; i < self->output_stream_count; ++i) {
		if (!self->used_output_streams[i]) {
			id = i;
			self->used_output_streams[i] = true;
			break;
		}
	}
	if (id + 1 > 0xF || params->channels > 16) {
		if (id != 0xFF) {
			self->used_output_streams[id] = false;
		}
		kfree(buffer, params->preferred_buffer_size);
		return NULL;
	}
	StreamDesc* stream = &self->output_streams[id];

	kprintf("available output paths:\n");
	for (usize i = 0; i < self->signal_paths_len; ++i) {
		SignalPath* path = &self->signal_paths[i];
		Widget* start = path->widgets[0];
		assert(start->type == WIDGET_AUDIO_PIN_COMPLEX);
		Widget* end = path->widgets[path->len - 1];
		if (end->type != WIDGET_AUDIO_OUT) {
			continue;
		}
		if (start->conf) {
			u8 location = start->conf >> 24 & 0b111111;

			u8 specific = location & 0b1111;
			u8 where = location >> 4;

			const char* specific_str;
			if (specific == 0) {
				specific_str = "N/A";
			}
			else if (specific == 1) {
				specific_str = "Rear";
			}
			else if (specific == 2) {
				specific_str = "Front";
			}
			else if (specific == 3) {
				specific_str = "Left";
			}
			else if (specific == 4) {
				specific_str = "Right";
			}
			else if (specific == 5) {
				specific_str = "Top";
			}
			else if (specific == 6) {
				specific_str = "Bottom";
			}
			else if (specific == 7) {
				if (where == 0) {
					specific_str = "Rear Panel";
				}
				else {
					specific_str = "Specific";
				}
			}
			else {
				specific_str = "Unknown";
			}

			if (specific == 2)
				kprintf("%u: %s\n", i, specific_str);
		}
		else {
			kprintf("unknown start\n");
		}
	}

	// todo choose
	assert(self->signal_paths_len);
	usize index = 0;
	assert(index < self->signal_paths_len);
	Widget* pin = self->signal_paths[index].widgets[0];
	assert(pin->type == WIDGET_AUDIO_PIN_COMPLEX);
	Widget* output = self->signal_paths[index].widgets[self->signal_paths[index].len - 1];
	assert(output->type == WIDGET_AUDIO_OUT);

	kprintf("path len: %u, setting amp params\n", self->signal_paths[index].len);
	for (usize i = 0; i < self->signal_paths[index].len; ++i) {
		Widget* widget = self->signal_paths[index].widgets[i];
		u32 amp_res = hda_send_cmd_await(self, PARAM_OUTPUT_AMP_CAPABILITIES, CMD_GET_PARAMETER, widget->nid, widget->cid).response;

		u8 out_offset = amp_res & 0x7F;
		u8 out_num_steps = amp_res >> 8 & 0x7F;
		u8 out_step_size = amp_res >> 16 & 0x7F;

		// set_output_amp | set_input_amp | set_left_amp | set_right_amp | 7bit_gain
		u16 out_data = 1 << 15 | 1 << 14 | 1 << 13 | 1 << 12 | (out_offset);

		u8 out_data_low = out_data & 0xFF;
		u16 out_cmd = CMD_SET_AMP_GAIN_MUTE << 8 | (out_data >> 8);
		hda_send_cmd_await(self, out_data_low, out_cmd, widget->nid, widget->cid);
	}

	AudioStream* res = &self->output_stream_infos[id];
	memset(res, 0, sizeof(AudioStream));
	res->id = id;
	id += 1;

	// set 4bit stream num
	stream->ctl[2] &= 0x0F;
	stream->ctl[2] |= id << 4;
	// Interrupt On Completion Enable
	stream->ctl[0] |= 1 << 2;

	// total cyclic buffer size
	stream->cbl = params->preferred_buffer_size;

	res->single_buffer_size = params->preferred_buffer_size / 2;
	res->buffer_size = params->preferred_buffer_size;

	u16 entries = params->preferred_buffer_size / PAGE_SIZE;

	// last valid index in the buffer descriptor list (1 == 2 entries) 8bit
	stream->lvi &= ~0xFF;
	stream->lvi |= entries - 1;

	u8 sample_value;
	switch (params->format) {
		case SND_PCM_INVALID:
			__builtin_unreachable();
		case SND_PCM_8:
			sample_value = 0;
			break;
		case SND_PCM_16NE:
			sample_value = 1;
			break;
	}
	u8 sample_rate_value;
	u16 sample_rate_base;
	if (params->sample_rate <= 6000) {
		sample_rate_value = 7;
		sample_rate_base = 0;
		params->sample_rate = 6000;
	}
	else if (params->sample_rate <= 6300) {
		sample_rate_value = 6;
		// 44.1khz base
		sample_rate_base = 1 << 14;
		params->sample_rate = 6300;
	}
	else if (params->sample_rate <= 6900) {
		sample_rate_value = 6;
		sample_rate_base = 0;
		params->sample_rate = 6857;
	}
	else if (params->sample_rate <= 9000) {
		sample_rate_value = 5;
		sample_rate_base = 0;
		params->sample_rate = 9000;
	}
	else if (params->sample_rate <= 9600) {
		sample_rate_value = 4;
		sample_rate_base = 0;
		params->sample_rate = 6900;
	}
	else if (params->sample_rate <= 11025) {
		sample_rate_value = 3;
		sample_rate_base = 0;
		params->sample_rate = 11025;
	}
	else if (params->sample_rate <= 16000) {
		sample_rate_value = 2;
		sample_rate_base = 0;
		params->sample_rate = 16000;
	}
	else if (params->sample_rate <= 22050) {
		sample_rate_value = 1;
		sample_rate_base = 1 << 14;
		params->sample_rate = 22050;
	}
	else if (params->sample_rate <= 24000) {
		sample_rate_value = 1;
		sample_rate_base = 0;
		params->sample_rate = 24000;
	}
	else if (params->sample_rate <= 32000) {
		sample_rate_value = 2;
		sample_rate_base = 1 << 14;
		params->sample_rate = 32000;
	}
	else if (params->sample_rate <= 44100) {
		sample_rate_value = 0;
		sample_rate_base = 1 << 14;
		params->sample_rate = 44100;
	}
	else {
		sample_rate_value = 0;
		sample_rate_base = 0;
		params->sample_rate = 48000;
	}

	u32 fmt_value = (params->channels - 1) | (sample_value << 4) | (sample_rate_value << 8) | (sample_rate_base);
	stream->fmt &= 0x8080;
	stream->fmt = fmt_value;

	if (!stream->bdpl && !stream->bdpu) {
		Page* buffer_desc_list_page = pmalloc(1);
		assert(buffer_desc_list_page);
		stream->bdpl = buffer_desc_list_page->phys;
		stream->bdpu = buffer_desc_list_page->phys >> 32;
	}

	usize buffer_desc_phys = stream->bdpl | (u64) stream->bdpu << 32;

	BufferDesc* buffer_desc_list = (BufferDesc*) to_virt(buffer_desc_phys);

	u16 data1 = fmt_value;
	u8 data1_low = data1 & 0xFF;
	u16 cmd1 = CMD_SET_CONVERTER_FORMAT << 8 | (data1 >> 8);

	hda_send_cmd_await(self, data1_low, cmd1, output->nid, output->cid);

	// 4bit_channel | 4bit_stream
	u8 channel_data = 0 | (id << 4);
	hda_send_cmd_await(self, channel_data, CMD_SET_CONVERTER_CHANNEL, output->nid, output->cid);

	// out_enable
	u8 pin_control = 1 << 6;
	hda_send_cmd_await(self, pin_control, CMD_SET_PIN_CONTROL, pin->nid, pin->cid);

	// external amp capable
	if (pin->pin_caps & 1 << 16) {
		// enable external amp
		hda_send_cmd_await(self, 1 << 0, CMD_SET_EAPD, pin->nid, pin->cid);
	}

	// power output on (D0)
	hda_send_cmd_await(self, 0, CMD_SET_POWER_STATE, output->nid, output->cid);

	for (usize i = 0; i < entries; ++i) {
		buffer_desc_list[i].addr = to_phys_generic(offset(buffer, void*, i * PAGE_SIZE));
		buffer_desc_list[i].len = PAGE_SIZE;
		buffer_desc_list[i].ioc = 0;
	}
	buffer_desc_list[entries / 2].ioc = 1;
	buffer_desc_list[entries - 1].ioc = 1;

	res->write_to_end = false;
	res->buffer = buffer;
	res->write_ptr = buffer;

	return res;
}

static bool snd_destroy_stream(SndDev* snd_self, AudioStream* snd_stream) {
	Controller* self = container_of(snd_self, Controller, snd_dev);

	//StreamDesc* stream = &self->output_streams[snd_stream->id];

	snd_self->play(snd_self, snd_stream, false);

	kfree(snd_stream->buffer, snd_stream->buffer_size);

	self->used_output_streams[snd_stream->id] = false;

	return true;
}

static bool snd_play(SndDev* snd_self, AudioStream* snd_stream, bool play) {
	Controller* self = container_of(snd_self, Controller, snd_dev);

	self->snd_dev.user_callback(snd_stream->buffer, snd_stream->buffer_size, self->snd_dev.user_data);

	StreamDesc* stream = &self->output_streams[snd_stream->id];

	if (play) {
		stream->ctl[0] |= 1 << 1;
	}
	else {
		stream->ctl[0] &= ~(1 << 1);
	}

	return true;
}

static bool snd_write(SndDev*, AudioStream* stream, const void* data, usize len) {
	if (len > stream->buffer_size) {
		return false;
	}

	usize written = stream->write_ptr - stream->buffer;
	usize write_before_wrap = MIN(len, stream->buffer_size - written);
	memcpy(stream->write_ptr, data, write_before_wrap);
	if (written + write_before_wrap == stream->buffer_size) {
		stream->write_ptr = stream->buffer;
	}
	memcpy(stream->write_ptr, data, len - write_before_wrap);
	stream->write_ptr += len - write_before_wrap;
	return true;
}

static void* sound_ptr = NULL;
static void* sound_start = NULL;
static void* sound_end = NULL;

static void callback(void* ptr, usize len, void* userdata) {
	if (sound_ptr >= sound_end) {
		sound_ptr = sound_start;
	}

	usize remaining = sound_end - sound_ptr;
	memcpy(ptr, sound_ptr, MIN(len, remaining));
	sound_ptr = offset(sound_ptr, void*, len);
}

static void hda_init(PciDev* dev) {
	kprintf("hda init\n");

	usize bar0 = pci_map_bar(dev->hdr0, 0);
	void* virt = to_virt(bar0);

	dev->hdr0->common.command |= PCI_CMD_MEM_SPACE | PCI_CMD_BUS_MASTER | PCI_CMD_INT_DISABLE;

	volatile Registers* regs = (volatile Registers*) virt;

	if (!GCAP_64OK(regs->gcap)) {
		kprintf("error: controller doesn't support 64bit\n");
		return;
	}

	regs->gctl &= ~GCTL_CRST_V;
	while (regs->gctl & GCTL_CRST_V);
	regs->gctl |= GCTL_CRST_V;
	while (!(regs->gctl & GCTL_CRST_V));

	udelay(521);

	// interrupt every n + 1 response
	regs->rintcnt |= 255;

	u8 corb_supported = CORBSIZE_CAP(regs->corbsize);
	u8 rirb_supported = CORBSIZE_CAP(regs->rirbsize);

	u16 corb_size = 2;
	u16 rirb_size = 2;

	if (corb_supported & 0b100) {
		corb_size = 256;
		if (corb_supported != 0b100) {
			regs->corbsize |= CORBSIZE_SIZE_V(0b10);
		}
	}
	else if (corb_supported & 0b10) {
		corb_size = 16;
		if (corb_supported != 0b10) {
			regs->corbsize |= CORBSIZE_SIZE_V(1);
		}
	}
	if (rirb_supported & 0b100) {
		rirb_size = 256;
		if (rirb_supported != 0b100) {
			regs->rirbsize |= CORBSIZE_SIZE_V(0b10);
		}
	}
	else if (rirb_supported & 0b10) {
		rirb_size = 16;
		if (rirb_supported != 0b10) {
			regs->rirbsize |= CORBSIZE_SIZE_V(1);
		}
	}

	kprintf("using %u corb entries and %u rirb entries\n", corb_size, rirb_size);

	Page* corb_page = pmalloc(1);
	assert(corb_page);
	void* corb = to_virt(corb_page->phys);
	regs->corblbase = corb_page->phys;
	regs->corbubase = corb_page->phys >> 32;

	regs->corbrp |= CORBRP_CORBRPRST_V;
	while (!CORBRP_CORBRPRST(regs->corbrp));
	regs->corbrp &= ~CORBRP_CORBRPRST_V;
	while (CORBRP_CORBRPRST(regs->corbrp));
	regs->corbwp &= 0xFF00;
	regs->corbctl |= CORBCTL_CORBRUN_V(1);
	while(!CORBCTL_CORBRUN(regs->corbctl));

	Page* rirb_page = pmalloc(1);
	assert(rirb_page);
	volatile RirbEntry* rirb = (volatile RirbEntry*) to_virt(rirb_page->phys);
	regs->rirblbase = rirb_page->phys;
	regs->rirbubase = rirb_page->phys >> 32;

	regs->rirbwp |= CORBRP_CORBRPRST_V;
	regs->rirbctl |= CORBCTL_CORBRUN_V(1);
	while(!CORBCTL_CORBRUN(regs->rirbctl));

	Controller* self = kcalloc(sizeof(Controller));
	assert(self);
	self->pci_dev = dev;
	self->regs = regs;
	self->corb = (u32*) corb;
	self->rirb = rirb;

	memset((void*) rirb, 0, PAGE_SIZE);
	memset(corb, 0, PAGE_SIZE);

	self->input_stream_count = GCAP_ISS(regs->gcap);
	self->output_stream_count = GCAP_OSS(regs->gcap);
	self->input_streams = (StreamDesc*) &regs->isd0;
	self->output_streams = offset(regs, StreamDesc*, 0x80 + self->input_stream_count * 0x20);

	u16 statests = regs->wakests;
	for (u8 i = 0; i < 15; ++i) {
		if (statests & 1 << i) {
			codec_enumerate(self, i);
		}
	}

	find_output_paths(self);

	assert(pci_irq_alloc(dev, 0, 0, PCI_IRQ_ALLOC_SHARED | PCI_IRQ_ALLOC_ALL));
	u32 irq = pci_irq_get(dev, 0);

	self->irq_handler.fn = irq_handler;
	self->irq_handler.userdata = self;
	assert(arch_irq_install(irq, &self->irq_handler, IRQ_INSTALL_SHARED) == IRQ_INSTALL_STATUS_SUCCESS);
	pci_enable_irqs(dev);

	// Global Interrupt Enable
	regs->intctl |= 1 << 31;
	for (usize i = 0; i < self->output_stream_count; ++i) {
		usize start = self->input_stream_count ? self->input_stream_count - 1 : 0;
		regs->intctl |= 1 << (start + i);
	}

	kprintf("controller ready\n");

	// todo
	memcpy(self->snd_dev.generic.name, "hda#unknown", sizeof("hda#unknown"));

	self->snd_dev.create_stream = snd_create_stream;
	self->snd_dev.destroy_stream = snd_destroy_stream;
	self->snd_dev.play = snd_play;
	self->snd_dev.write = snd_write;

	dev_add(&self->snd_dev.generic, DEVICE_TYPE_SND);

	/*Module song_mod = arch_get_module("output_raw");
	assert(song_mod.base);

	SndDev* s = &self->snd_dev;

	AudioParams params = {
		.sample_rate = 44100,
		.format = SND_PCM_16NE,
		.channels = 2,
		.preferred_buffer_size = 0
	};
	AudioStream* stream = s->create_stream(s, &params);
	s->user_callback = callback;
	sound_start = song_mod.base;

	sound_ptr = sound_start;
	sound_end = offset(song_mod.base, void*, song_mod.size);
	assert(s->play(s, stream, true));*/
}

static PciDriver pci_driver = {
	.match = PCI_MATCH_CLASS | PCI_MATCH_SUBCLASS,
	.dev_class = 4,
	.dev_subclass = 3,
	.load = hda_init
};

PCI_DRIVER(hda_driver, &pci_driver);
