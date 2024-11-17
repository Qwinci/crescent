#include "events.hpp"
#include "acpi.hpp"
#include "manually_destroy.hpp"
#include "mem/register.hpp"
#include "qacpi/event.hpp"
#include "qacpi/ns.hpp"
#include "qacpi/resources.hpp"
#include "sleep.hpp"

namespace acpi {
	namespace ec_cmd {
		static constexpr u8 RD_EC = 0x80;
		static constexpr u8 WR_EC = 0x81;
		static constexpr u8 BE_EC = 0x82;
		static constexpr u8 BD_EC = 0x83;
		static constexpr u8 QR_EC = 0x84;
	}

	namespace ec_control {
		static constexpr BitField<u8, bool> OBF {0, 1};
		static constexpr BitField<u8, bool> IBF {1, 1};
		static constexpr BitField<u8, bool> CMD {3, 1};
		static constexpr BitField<u8, bool> BURST {4, 1};
		static constexpr BitField<u8, bool> SCI_EVT {5, 1};
	}

	static constexpr u8 EC_BURST_ACK = 0x90;

	struct Ec {
		qacpi::Address data;
		qacpi::Address control;
		qacpi::NamespaceNode* node;

		void wait_for_space() const {
			while (true) {
				u64 status_int = 0;
				assert(qacpi::read_from_addr(control, status_int) == qacpi::Status::Success);
				BitValue<u8> status {static_cast<u8>(status_int)};
				if (!(status & ec_control::IBF)) {
					break;
				}
			}
		}

		void wait_for_data() const {
			while (true) {
				u64 status_int = 0;
				assert(qacpi::read_from_addr(control, status_int) == qacpi::Status::Success);
				BitValue<u8> status {static_cast<u8>(status_int)};
				if (status & ec_control::OBF) {
					break;
				}
			}
		}

		void wait_for_burst(bool enabled) const {
			while (true) {
				u64 status_int = 0;
				assert(qacpi::read_from_addr(control, status_int) == qacpi::Status::Success);
				BitValue<u8> status {static_cast<u8>(status_int)};
				if (enabled) {
					if (status & ec_control::BURST) {
						break;
					}
				}
				else {
					if (!(status & ec_control::BURST)) {
						break;
					}
				}
			}
		}

		void enable_burst() {
			write_control(ec_cmd::BE_EC);
			assert(read_data() == EC_BURST_ACK);
			wait_for_burst(true);
		}

		void disable_burst() {
			write_control(ec_cmd::BD_EC);
			wait_for_burst(false);
		}

		void write_control(u8 value) {
			wait_for_space();
			qacpi::write_to_addr(control, value);
		}

		void write_data(u8 value) {
			wait_for_space();
			qacpi::write_to_addr(data, value);
		}

		[[nodiscard]] u8 read_data() const {
			wait_for_data();
			u64 value;
			assert(qacpi::read_from_addr(data, value) == qacpi::Status::Success);
			return value;
		}

		u8 read(u8 offset) {
			write_control(ec_cmd::RD_EC);
			write_data(offset);
			return read_data();
		}

		void write(u8 offset, u8 value) {
			write_control(ec_cmd::WR_EC);
			write_data(offset);
			write_data(value);
		}

		void on_gpe() {
			while (true) {
				u64 status_int = 0;
				assert(qacpi::read_from_addr(control, status_int) == qacpi::Status::Success);
				BitValue<u8> status {static_cast<u8>(status_int)};
				if (!(status & ec_control::SCI_EVT)) {
					return;
				}

				enable_burst();
				write_control(ec_cmd::QR_EC);
				u8 code = read_data();
				disable_burst();
				if (!code) {
					return;
				}

				println("ec query ", Fmt::Hex, code, Fmt::Reset);

				static constexpr char CHARS[] = "0123456789ABCDEF";
				char name[5] {'_', 'Q', CHARS[code >> 4], CHARS[code & 0xF]};

				auto res = qacpi::ObjectRef::empty();
				GLOBAL_CTX->evaluate(node, name, res);
			}
		}

		void clear_events() {
			for (int i = 0; i < 100; ++i) {
				enable_burst();
				write_control(ec_cmd::QR_EC);
				u8 code = read_data();
				disable_burst();
				if (!code) {
					break;
				}
				else if (i == 99) {
					println("[kernel][acpi]: warning: ec keeps generating events");
				}

				println("[kernel][acpi]: discarding ec query ", Fmt::Hex, code, Fmt::Reset);
			}
		}
	};

	Ec EC {};

	static qacpi::RegionSpaceHandler EC_HANDLER {
		.attach = [](qacpi::Context*, qacpi::NamespaceNode*) {
			return qacpi::Status::Success;
		},
		.detach = [](qacpi::Context*, qacpi::NamespaceNode*) {
			return qacpi::Status::Success;
		},
		.read = [](qacpi::NamespaceNode*, uint64_t offset, uint8_t size, uint64_t& res, void*) {
			if (size != 1) {
				println("[kernel][acpi]: invalid ec read access width ", size);
				return qacpi::Status::InvalidArgs;
			}
			else if (offset > 0xFF) {
				println("[kernel][acpi]: invalid ec read access offset ", offset);
				return qacpi::Status::InvalidArgs;
			}

			EC.enable_burst();
			res = EC.read(offset);
			EC.disable_burst();

			return qacpi::Status::Success;
		},
		.write = [](qacpi::NamespaceNode*, uint64_t offset, uint8_t size, uint64_t value, void*) {
			if (size != 1) {
				println("[kernel][acpi]: invalid ec write access width ", size);
				return qacpi::Status::InvalidArgs;
			}
			else if (offset > 0xFF) {
				println("[kernel][acpi]: invalid ec write access offset ", offset);
				return qacpi::Status::InvalidArgs;
			}

			EC.enable_burst();
			EC.write(offset, value);
			EC.disable_burst();

			return qacpi::Status::Success;
		},
		.id = qacpi::RegionSpace::EmbeddedControl
	};

	struct [[gnu::packed]] Ecdt {
		SdtHeader hdr;
		qacpi::Address ec_control;
		qacpi::Address ec_data;
		u32 uid;
		u8 gpe;
		char ec_id[];
	};

	bool EARLY_EC = false;
	ManuallyDestroy<qacpi::events::Context> EVENT_CTX {};

	void early_events_init() {
		auto* ecdt = static_cast<Ecdt*>(get_table("ECDT"));
		if (!ecdt) {
			return;
		}

		EC.data = ecdt->ec_data;
		EC.control = ecdt->ec_control;
		EC.node = GLOBAL_CTX->find_node(nullptr, ecdt->ec_id, false);
		assert(EC.node);
		EC.clear_events();

		auto status = EVENT_CTX->enable_gpe(
			ecdt->gpe,
			qacpi::events::GpeTrigger::Edge,
			[](void*) {
				EC.on_gpe();
			},
			nullptr);
		assert(status == qacpi::Status::Success);

		GLOBAL_CTX->register_address_space_handler(&EC_HANDLER);

		EARLY_EC = true;
	}

	static void on_power_button_press() {
		println("[kernel][acpi]: power button press, do shutdown");
		acpi::enter_sleep_state(SleepState::S5);
	}

	void events_init() {
		// todo proper locking in qacpi to fix races between gpe init and gpe handling
		IrqGuard irq_guard {};

		assert(EVENT_CTX->enable_events_from_ns(*GLOBAL_CTX) == qacpi::Status::Success);

		auto status = EVENT_CTX->enable_fixed_event(
			qacpi::events::FixedEvent::PowerButton,
			[](void*) {
				on_power_button_press();
			},
			nullptr);
		assert(status == qacpi::Status::Success || status == qacpi::Status::Unsupported);

		qacpi::EisaId power_button_id {"PNP0C0C"};
		GLOBAL_CTX->discover_nodes(
			GLOBAL_CTX->get_root(),
			&power_button_id,
			1,
			[](qacpi::Context& ctx, qacpi::NamespaceNode* node) {
				auto status = EVENT_CTX->install_notify_handler(
					node,
					[](void*, qacpi::NamespaceNode*, uint64_t value) {
						if (value == 0x80) {
							on_power_button_press();
						}
					},
					nullptr);
				assert(status == qacpi::Status::Success);

				return qacpi::IterDecision::Continue;
			});

		if (EARLY_EC) {
			return;
		}

		qacpi::Address data_reg {};
		data_reg.space_id = qacpi::RegionSpace::SystemIo;
		qacpi::Address control_reg {};
		control_reg.space_id = qacpi::RegionSpace::SystemIo;

		qacpi::EisaId ec_id {"PNP0C09"};
		GLOBAL_CTX->discover_nodes(
			GLOBAL_CTX->get_root(),
			&ec_id,
			1,
			[&](qacpi::Context& ctx, qacpi::NamespaceNode* node) {
				auto crs_obj = qacpi::ObjectRef::empty();
				auto status = ctx.evaluate(node, "_CRS", crs_obj);
				if (status != qacpi::Status::Success) {
					return qacpi::IterDecision::Continue;
				}
				auto crs_buffer = crs_obj->get<qacpi::Buffer>();
				assert(crs_buffer);

				int index = 0;
				size_t offset = 0;
				while (true) {
					qacpi::Resource res {};
					status = qacpi::resource_parse(
						crs_buffer->data(),
						crs_buffer->size(),
						offset,
						res);
					if (status == qacpi::Status::EndOfResources) {
						break;
					}

					if (auto io = res.get<qacpi::IoPortDescriptor>()) {
						if (index == 0) {
							data_reg.address = io->min_base;
							data_reg.reg_bit_width = io->length * 8;
							++index;
						}
						else {
							control_reg.address = io->min_base;
							control_reg.reg_bit_width = io->length * 8;
							++index;
							break;
						}
					}
					else if (auto fixed_io = res.get<qacpi::FixedIoPortDescriptor>()) {
						if (index == 0) {
							data_reg.address = fixed_io->base;
							data_reg.reg_bit_width = fixed_io->length * 8;
							++index;
						}
						else {
							control_reg.address = fixed_io->base;
							control_reg.reg_bit_width = fixed_io->length * 8;
							++index;
							break;
						}
					}
					else {
						println("unhandled res type ", res.index());
						continue;
					}
				}

				if (index != 2) {
					println("[kernel][acpi]: failed to find resources for ec");
					return qacpi::IterDecision::Continue;
				}

				auto gpe_obj = qacpi::ObjectRef::empty();
				status = ctx.evaluate(node, "_GPE", gpe_obj);
				assert(status == qacpi::Status::Success);
				assert(gpe_obj->get<uint64_t>());
				auto gpe_index = gpe_obj->get_unsafe<uint64_t>();

				EC.data = data_reg;
				EC.control = control_reg;
				EC.node = node;
				EC.clear_events();

				status = EVENT_CTX->enable_gpe(
					gpe_index,
					qacpi::events::GpeTrigger::Edge,
					[](void*) {
						EC.on_gpe();
					},
					nullptr);
				assert(status == qacpi::Status::Success);

				GLOBAL_CTX->register_address_space_handler(&EC_HANDLER);

				return qacpi::IterDecision::Break;
			});
	}
}
