#include "pci.hpp"
#include "manually_init.hpp"
#include "manually_destroy.hpp"
#include "qacpi/context.hpp"
#include "qacpi/ns.hpp"
#include "qacpi/resources.hpp"
#include "vector.hpp"

namespace acpi {
	extern ManuallyInit<qacpi::Context> GLOBAL_CTX;

	struct PciRouting {
		u32 address;
		u32 pin;
		qacpi::NamespaceNode* source;
		u32 source_index;
	};

	struct PciRootBus {
		u16 seg;
		u8 bus;
		kstd::vector<PciRouting> routings;
	};

	namespace {
		ManuallyDestroy<kstd::vector<PciRootBus>> PCI_ROOT_BUSES;
	}

	kstd::optional<LegacyPciIrq> get_legacy_pci_irq(u16 seg, u8 bus, u8 device, u8 pci_pin) {
		for (auto& root_bus : *PCI_ROOT_BUSES) {
			if (root_bus.seg != seg || root_bus.bus != bus) {
				continue;
			}

			for (auto& routing : root_bus.routings) {
				u32 gsi = routing.source_index;
				u16 routing_device = routing.address >> 16;

				bool edge_triggered = true;
				bool active_high = true;

				if (routing.source) {
					auto crs_obj = qacpi::ObjectRef::empty();
					auto status = GLOBAL_CTX->evaluate(routing.source, "_CRS", crs_obj);
					if (status != qacpi::Status::Success) {
						println("[kernel][qacpi]: failed to evaluate _CRS: ", qacpi::status_to_str(status));
						return {};
					}
					auto buffer = crs_obj->get<qacpi::Buffer>();
					if (!buffer) {
						println("[kernel][qacpi]: invalid _CRS object type");
						return {};
					}

					usize offset = 0;
					while (true) {
						qacpi::Resource res;
						status = qacpi::resource_parse(buffer->data->data, buffer->data->size, offset, res);
						if (status == qacpi::Status::EndOfResources) {
							break;
						}
						else if (status != qacpi::Status::Success) {
							println("[kernel][qacpi]: failed to parse resources: ", qacpi::status_to_str(status));
							return {};
						}

						if (auto irq = res.get<qacpi::IrqDescriptor>()) {
							edge_triggered = irq->info & qacpi::IRQ_INFO_EDGE_TRIGGERED;
							active_high = !(irq->info & qacpi::IRQ_INFO_ACTIVE_LOW);
							for (u32 i = 0; i < 16; ++i) {
								if (irq->mask_bits & 1 << i) {
									gsi = i;
									break;
								}
							}
						}
						else if (auto ext_irq = res.get<qacpi::ExtendedIrqDescriptor>()) {
							edge_triggered = ext_irq->info & qacpi::EXT_IRQ_INFO_EDGE_TRIGGERED;
							active_high = !(ext_irq->info & qacpi::EXT_IRQ_INFO_ACTIVE_LOW);
							assert(ext_irq->irq_table_length);
							gsi = ext_irq->irq_table[0];
							break;
						}
					}
				}

				if (routing_device == device && routing.pin + 1 == pci_pin) {
					return {LegacyPciIrq {
						.gsi = gsi,
						.edge_triggered = edge_triggered,
						.active_high = active_high
					}};
				}
			}
		}

		return {};
	}

	bool get_pci_routing(qacpi::NamespaceNode* node, kstd::vector<PciRouting>& routings) {
		auto ret = qacpi::ObjectRef::empty();
		auto status = GLOBAL_CTX->evaluate(node, "_PRT", ret);
		if (status != qacpi::Status::Success) {
			println("[kernel][qacpi]: failed to get pci routing: ", qacpi::status_to_str(status));
			return false;
		}
		qacpi::Package* pkg;
		if (!(pkg = ret->get<qacpi::Package>())) {
			println("[kernel][qacpi]: error: _PRT is not a package");
			return false;
		}

		for (u32 i = 0; i < pkg->size(); ++i) {
			auto elem = GLOBAL_CTX->get_package_element(ret, i);
			if (!elem) {
				println("[kernel][qacpi]: error: failed to resolve _PRT package element");
				continue;
			}
			if (elem->get_unsafe<qacpi::Package>().size() != 4) {
				println("[kernel][qacpi]: error: _PRT element contains invalid number of elements");
				return false;
			}

			u64* address;
			if (!(address = GLOBAL_CTX->get_package_element(elem, 0)->get<uint64_t>())) {
				println("[kernel][qacpi]: error: _PRT address is not a number");
				return false;
			}

			u64* pin;
			if (!(pin = GLOBAL_CTX->get_package_element(elem, 1)->get<uint64_t>())) {
				println("[kernel][qacpi]: error: _PRT pin is not a number");
				return false;
			}

			qacpi::NamespaceNode* source = nullptr;
			auto source_obj = GLOBAL_CTX->get_package_element(elem, 2);
			if (source_obj->get<qacpi::Device>()) {
				source = source_obj->node;
			}
			else if (!source_obj->get<uint64_t>()) {
				println("[kernel][qacpi]: error: _PRT source is not a number or a device");
				return false;
			}

			uint64_t* source_index;
			if (!(source_index = GLOBAL_CTX->get_package_element(elem, 3)->get<uint64_t>())) {
				println("[kernel][qacpi]: error: _PRT source index is not a number");
				return false;
			}

			routings.push({
				.address = static_cast<u32>(*address),
				.pin = static_cast<u32>(*pin),
				.source = source,
				.source_index = static_cast<u32>(*source_index)
			});
		}

		return true;
	}

	void pci_init() {
		auto sb = GLOBAL_CTX->get_root()->get_child("_SB_");

		constexpr qacpi::EisaId PCI_IDS[] {
			qacpi::PCI_ID,
			qacpi::PCIE_ID
		};

		GLOBAL_CTX->discover_nodes(
			sb,
			PCI_IDS,
			2,
			[](qacpi::Context& ctx, qacpi::NamespaceNode* node) {
				auto seg_obj = qacpi::ObjectRef::empty();
				auto status = ctx.evaluate(node, "_SEG", seg_obj);
				u16 seg = 0;
				if (status == qacpi::Status::Success) {
					if (!seg_obj->get<uint64_t>()) {
						println("[kernel][qacpi]: _SEG is not a number");
						return false;
					}
					seg = seg_obj->get_unsafe<uint64_t>();
				}
				else if (status != qacpi::Status::MethodNotFound) {
					println("[kernel][qacpi]: failed to execute _SEG: ", qacpi::status_to_str(status));
					return false;
				}

				auto bbn_obj = qacpi::ObjectRef::empty();
				status = ctx.evaluate(node, "_BBN", bbn_obj);
				u8 bbn = 0;
				if (status == qacpi::Status::Success) {
					if (!bbn_obj->get<uint64_t>()) {
						println("[kernel][qacpi]: _BBN is not a number");
						return false;
					}
					bbn = bbn_obj->get_unsafe<uint64_t>();
				}
				else if (status != qacpi::Status::MethodNotFound) {
					println("[kernel][qacpi]: failed to execute _BBN: ", qacpi::status_to_str(status));
					return false;
				}

				kstd::vector<PciRouting> routings;
				if (!get_pci_routing(node, routings)) {
					return false;
				}

				PCI_ROOT_BUSES->push({
					.seg = seg,
					.bus = bbn,
					.routings {std::move(routings)}
				});

				return false;
			});
	}
}
