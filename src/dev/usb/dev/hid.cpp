#include "arch/paging.hpp"
#include "utils/driver.hpp"
#include "mem/vspace.hpp"
#include "sched/process.hpp"
#include "sys/event_queue.hpp"

struct [[gnu::packed]] HidDesc {
	u8 type;
	u16 length;
};

namespace {
	constexpr u8 HID_REPORT_TYPE = 0x22;
}

namespace hid_reg {
	static constexpr u8 GET_REPORT = 1;
	static constexpr u8 GET_IDLE = 2;
	static constexpr u8 GET_PROTOCOL = 3;
	static constexpr u8 SET_REPORT = 9;
	static constexpr u8 SET_IDLE = 0xA;
	static constexpr u8 SET_PROTOCOL = 0xB;
}

struct [[gnu::packed]] HidDescriptor {
	u8 length;
	u8 descriptor_type;
	u16 bcd_hid;
	u8 country_code;
	u8 num_descriptors;
	HidDesc descs[];
};

namespace {
	constexpr u8 ITEM_SIZES[] {
		0,
		1,
		2,
		4
	};

	enum class Type : u8 {
		Main,
		Global,
		Local,
		Reserved
	};

	enum class MainTag : u8 {
		Input = 0b1000,
		Output = 0b1001,
		Feature = 0b1011,
		Collection = 0b1010,
		EndCollection = 0b1100
	};

	enum class GlobalTag : u8 {
		UsagePage = 0,
		LogicalMin = 1,
		LogicalMax = 0b10,
		PhysicalMin = 0b11,
		PhysicalMax = 0b100,
		UnitExponent = 0b101,
		Unit = 0b110,
		ReportSize = 0b111,
		ReportId = 0b1000,
		ReportCount = 0b1001,
		Push = 0b1010,
		Pop = 0b1011
	};

	enum class LocalTag : u8 {
		Usage = 0,
		UsageMin = 1,
		UsageMax = 0b10,
		DesignatorIndex = 0b11,
		DesignatorMin = 0b100,
		DesignatorMax = 0b101,
		StringIndex = 0b111,
		StringMin = 0b1000,
		StringMax = 0b1001,
		Delimiter = 0b1010
	};

	constexpr i32 sign_extent(u32 value, u8 bits) {
		u32 mask = 1U << (bits - 1);
		return static_cast<i32>((value ^ mask) - mask);
	}

	struct Field {
		u32 usage;
		u32 bit_size;
		u32 min;
		u32 max;
		bool is_signed;
		bool is_index;
		bool is_relative;
		bool pad;
	};

	struct Report {
		kstd::optional<u32> id;
		kstd::vector<Field> fields;
	};
}

static kstd::vector<Report> parse_hid_report(const u8* report, usize report_length) {
	struct Global {
		u32 usage_page = 0;
		u32 logical_min = 0;
		i32 logical_min_signed = 0;
		u32 logical_max = 0;
		i32 logical_max_signed = 0;
		u32 unit_exponent = 0;
		u32 unit = 0;
		u32 report_size = 0;
		u32 report_count = 0;
	};

	struct Local {
		kstd::vector<u32> usages {};
		kstd::optional<u32> usage_min {};
		kstd::optional<u32> usage_max {};
	};

	kstd::vector<Global> global_stack;
	Global global {};
	Local local {};

	kstd::vector<Report> reports;
	Report result {};

	for (usize i = 0; i < report_length;) {
		u8 first = report[i++];
		u8 size = ITEM_SIZES[first & 0b11];
		auto type = static_cast<Type>(first >> 2 & 0b11);
		u8 tag = first >> 4;

		if (size == 2 && type == Type::Reserved && tag == 0b1111) {
			u16 actual_size = report[i] | report[i + 1] << 8;
			u16 actual_tag = report[i + 2] | report[i + 3] << 8;
			i += 4;
			i += actual_size;
			continue;
		}

		u32 data = 0;
		memcpy(&data, report + i, size);

		switch (type) {
			case Type::Main:
				switch (static_cast<MainTag>(tag)) {
					case MainTag::Input:
					{
						bool is_variable = data & 1 << 1;
						bool is_relative = data & 1 << 2;

						for (u32 j = 0; j < global.report_count; ++j) {
							u32 usage;
							bool no_usage = false;
							if (!local.usages.is_empty()) {
								usage = local.usages[0];
								if (local.usages.size() > 1) {
									local.usages.remove(0);
								}
							}
							else {
								if (local.usage_min || local.usage_max) {
									assert(local.usage_min);
									assert(local.usage_max);
									auto diff = local.usage_max.value() - local.usage_min.value();
									if (j < diff) {
										usage = local.usage_min.value() + j;
									}
									else {
										usage = local.usage_max.value();
									}
								}
								else {
									usage = 0;
									no_usage = true;
								}
							}

							bool is_signed = global.logical_min_signed < 0 || global.logical_max_signed < 0;

							result.fields.push(Field {
								.usage = usage,
								.bit_size = global.report_size,
								.min = global.logical_min,
								.max = global.logical_max,
								.is_signed = is_signed,
								.is_index = !is_variable,
								.is_relative = is_relative,
								.pad = no_usage
							});
						}
						break;
					}
					case MainTag::Output:
					case MainTag::Feature:
					case MainTag::Collection:
					case MainTag::EndCollection:
					default:
						break;
				}
				local = {};
				break;
			case Type::Global:
				switch (static_cast<GlobalTag>(tag)) {
					case GlobalTag::UsagePage:
						global.usage_page = data;
						break;
					case GlobalTag::LogicalMin:
						global.logical_min = data;
						global.logical_min_signed = sign_extent(data, size * 8);
						break;
					case GlobalTag::LogicalMax:
						global.logical_max = data;
						global.logical_max_signed = sign_extent(data, size * 8);
						break;
					case GlobalTag::PhysicalMin:
					case GlobalTag::PhysicalMax:
						break;
					case GlobalTag::UnitExponent:
						global.unit_exponent = data;
						break;
					case GlobalTag::Unit:
						global.unit = data;
						break;
					case GlobalTag::ReportSize:
						global.report_size = data;
						break;
					case GlobalTag::ReportId:
						result.id = data;
						if (!result.fields.is_empty()) {
							reports.push(std::move(result));
							result = {};
						}
						break;
					case GlobalTag::ReportCount:
						global.report_count = data;
						break;
					case GlobalTag::Push:
						global_stack.push(global);
						break;
					case GlobalTag::Pop:
						global = global_stack.pop().value();
						break;
					default:
						break;
				}
				break;
			case Type::Local:
				switch (static_cast<LocalTag>(tag)) {
					case LocalTag::Usage:
						if (size == 4) {
							local.usages.push(data);
						}
						else {
							local.usages.push(data | global.usage_page << 16);
						}
						break;
					case LocalTag::UsageMin:
						if (size == 4) {
							local.usage_min = data;
						}
						else {
							local.usage_min = data | global.usage_page << 16;
						}
						break;
					case LocalTag::UsageMax:
						if (size == 4) {
							local.usage_max = data;
						}
						else {
							local.usage_max = data | global.usage_page << 16;
						}
						break;
					case LocalTag::DesignatorIndex:
					case LocalTag::DesignatorMin:
					case LocalTag::DesignatorMax:
					case LocalTag::StringIndex:
					case LocalTag::StringMin:
					case LocalTag::StringMax:
					case LocalTag::Delimiter:
					default:
						break;
				}
				break;
			case Type::Reserved:
				break;
		}

		i += size;
	}

	reports.push(std::move(result));

	return reports;
}

struct EventState {
	MouseEvent mouse;
	KeyEvent key[8];
	u32 key_count;
	bool mouse_modified;
	bool modifiers[8];
	kstd::vector<Scancode> prev;
	kstd::vector<Scancode> new_prev;
};

static void add_event(EventState& state, const Field& field, u32 value) {
	auto usage = field.usage;
	u32 page = usage >> 16;
	usage &= 0xFFFF;

	auto set_relative = [&](i32& res) {
		assert(field.is_relative);
		assert(field.is_signed);
		state.mouse_modified = true;
		res = sign_extent(value, field.bit_size);
	};

	if (page == 1) {
		switch (usage) {
			case 0x30:
				set_relative(state.mouse.x_movement);
				break;
			case 0x31:
				set_relative(state.mouse.y_movement);
				state.mouse.y_movement = -state.mouse.y_movement;
				break;
			case 0x38:
				set_relative(state.mouse.z_movement);
				break;
			default:
				break;
		}
	}
	else if (page == 7) {
		if (usage >= 224 && usage <= 231) {
			if (value) {
				if (!state.modifiers[usage - 224]) {
					state.modifiers[usage - 224] = true;
					state.key[state.key_count++] = {
						.code = static_cast<Scancode>(usage),
						.pressed = true
					};
				}
			}
			else {
				for (u32 i = 224; i < 231; ++i) {
					if (state.modifiers[i - 224]) {
						state.modifiers[i - 224] = false;
						state.key[state.key_count++] = {
							.code = static_cast<Scancode>(i),
							.pressed = false
						};
					}
				}
			}
		}
		else {
			assert(field.is_index);
			if (value >= 4) {
				state.new_prev.push(static_cast<Scancode>(value));
			}
		}
	}
	else if (page == 9) {
		switch (usage) {
			case 1:
				state.mouse_modified = true;
				state.mouse.left_pressed = value;
				break;
			case 2:
				state.mouse_modified = true;
				state.mouse.right_pressed = value;
				break;
			case 3:
				state.mouse_modified = true;
				state.mouse.middle_pressed = value;
				break;
			default:
				break;
		}
	}
}

static void finalize_event(EventState& state) {
	if (state.mouse_modified) {
		GLOBAL_EVENT_QUEUE.push({
			.type = EVENT_TYPE_MOUSE,
			.mouse = state.mouse
		});
	}
	for (u32 i = 0; i < state.key_count; ++i) {
		GLOBAL_EVENT_QUEUE.push({
			.type = EVENT_TYPE_KEY,
			.key = state.key[i]
		});
	}

	for (auto new_key : state.new_prev) {
		GLOBAL_EVENT_QUEUE.push({
			.type = EVENT_TYPE_KEY,
			.key {
				.code = new_key,
				.pressed = true
			}
		});
	}

	for (auto old_key : state.prev) {
		bool still_pressed = false;

		for (auto new_key : state.new_prev) {
			if (new_key == old_key) {
				still_pressed = true;
				break;
			}
		}

		if (!still_pressed) {
			GLOBAL_EVENT_QUEUE.push({
				.type = EVENT_TYPE_KEY,
				.key {
					.code = old_key,
					.pressed = false
				}
			});
		}
	}

	state.prev = std::move(state.new_prev);
}

static bool usb_hid_init(const usb::AssignedDevice& device) {
	println("[kernel][usb]: hid init");
	HidDescriptor* hid_desc = nullptr;

	auto& iface = device.interfaces[device.interface_index];

	for (auto& desc : iface.descs) {
		if (desc.type == usb::setup::desc_type::HID) {
			hid_desc = static_cast<HidDescriptor*>(desc.data);
			break;
		}
	}

	if (!hid_desc) {
		println("no hid desc");
		return false;
	}

	u16 report_length = 0;
	for (u32 i = 0; i < hid_desc->num_descriptors; ++i) {
		auto& desc = hid_desc->descs[i];
		if (desc.type == HID_REPORT_TYPE) {
			report_length = desc.length;
			break;
		}
	}

	if (!report_length) {
		println("no report length");
		return false;
	}

	auto report_buffer = UniquePhysical::create(ALIGNUP(report_length, PAGE_SIZE) / PAGE_SIZE).value();

	auto status = device.device.control({
		usb::Dir::FromDevice,
		usb::setup::Type::Standard,
		usb::setup::Rec::Interface,
		usb::setup::std_req::GET_DESCRIPTOR,
		HID_REPORT_TYPE << 8 | 0,
		device.interface_index,
		report_buffer.base,
		report_length
	});
	assert(status == usb::Status::Success);

	auto reports = parse_hid_report(to_virt<u8>(report_buffer.base), report_length);

	status = device.device.control({
		usb::Dir::ToDevice,
		usb::setup::Type::Class,
		usb::setup::Rec::Interface,
		hid_reg::SET_IDLE,
		0,
		device.interface_index,
		0,
		0
	});
	assert(status == usb::Status::Success);

	status = device.device.control({
		usb::Dir::ToDevice,
		usb::setup::Type::Class,
		usb::setup::Rec::Interface,
		hid_reg::SET_PROTOCOL,
		1,
		device.interface_index,
		0,
		0
	});
	assert(status == usb::Status::Success);

	assert(iface.eps.size() == 1);
	auto ep = iface.eps[0];
	auto num = ep.number();

	bool has_prefix = reports.size() > 1;

	usize total_bit_size = 0;
	for (auto& report : reports) {
		if (has_prefix) {
			assert(report.id);
		}
		for (auto& field : report.fields) {
			assert(field.bit_size <= 32);
			total_bit_size += field.bit_size;
		}
	}

	if (has_prefix) {
		total_bit_size += 8;
	}

	usize total_byte_size = (total_bit_size + 7) / 8;
	assert(PAGE_SIZE / total_byte_size >= 1);

	auto page = pmalloc(1);
	assert(page);
	auto virt = KERNEL_VSPACE.alloc(PAGE_SIZE);
	assert(virt);
	assert(KERNEL_PROCESS->page_map.map(
		reinterpret_cast<u64>(virt),
		page,
		PageFlags::Read | PageFlags::Write,
		CacheMode::Uncached));

	usize count = PAGE_SIZE / total_byte_size;

	kstd::vector<usb::normal::Packet> packets;
	packets.reserve(count);
	for (usize i = 0; i < count; ++i) {
		packets.push(usb::normal::Packet {
			usb::Dir::FromDevice,
			num,
			page + i * total_byte_size,
			static_cast<u16>(total_byte_size)
		});
	}

	count = device.device.normal_multiple(packets.data(), count);

	EventState state {};

	while (true) {
		for (usize i = 0; i < count; ++i) {
			auto* packet = &packets[i];
			packet->event.wait();
			if (packet->event.status == usb::Status::Detached) {
				println("[kernel][usb]: hid driver exiting due to a device detach");
				pfree(page, 1);
				KERNEL_PROCESS->page_map.unmap(reinterpret_cast<u64>(virt));
				KERNEL_VSPACE.free(virt, PAGE_SIZE);
				return true;
			}

			auto* ptr = static_cast<u8*>(virt) + i * total_byte_size;
			Report* report;
			if (has_prefix) {
				auto id = *ptr++;

				report = nullptr;
				for (auto& iter : reports) {
					if (iter.id.value() == id) {
						report = &iter;
						break;
					}
				}

				assert(report);
			}
			else {
				report = &reports[0];
			}

			u32 report_bit_offset = 0;

			auto get_value = [&](const Field& field) {
				u32 byte_offset = report_bit_offset / 8;

				u32 value = 0;
				for (u32 j = 0; j < field.bit_size;) {
					u32 bit_offset = (report_bit_offset + j) % 8;
					u32 bits = kstd::min(field.bit_size - j, 8U - bit_offset);

					u8 byte = ptr[byte_offset];
					byte >>= bit_offset;

					u8 mask = (1U << bits) - 1;
					byte &= mask;

					value |= byte << j;
					j += bits;
					++byte_offset;
				}

				return value;
			};

			state.mouse_modified = false;
			state.key_count = 0;

			for (auto& field : report->fields) {
				if (field.pad) {
					report_bit_offset += field.bit_size;
					continue;
				}

				u32 value = get_value(field);

				if (field.is_index) {
					if (value >= field.min && value <= field.max) {
						add_event(state, field, value);
					}
				}
				else {
					add_event(state, field, value);
				}

				report_bit_offset += field.bit_size;
			}

			finalize_event(state);

			assert(device.device.normal_one(packet));
		}
	}
}

static constexpr UsbDriver HID_DRIVER {
	.name = "USB HID driver",
	.init = usb_hid_init,
	.match = UsbMatch::InterfaceClass,
	.interface_class = 3
};

USB_DRIVER(HID_DRIVER);
