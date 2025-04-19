#include "kernel_dtb.hpp"
#include "dtb.hpp"

ManuallyInit<Dtb> DTB {};

void kernel_dtb_init(void* plain_dtb_void) {
	auto* plain_dtb = static_cast<dtb::Dtb*>(plain_dtb_void);

	DTB.initialize();

	auto root = plain_dtb->get_root();

	kstd::vector<kstd::pair<DtbNode*, dtb::Node>> traverse_stack {};
	traverse_stack.push({nullptr, root});

	while (!traverse_stack.is_empty()) {
		auto [parent, node] = traverse_stack.pop().value();

		auto* new_node = new DtbNode {};
		new_node->name = node.name;
		new_node->parent = parent;
		if (parent) {
			parent->children.push(new_node);
		}
		else {
			DTB->root = new_node;
		}

		bool has_interrupt_parent = false;

		auto* next_ptr = dtb::Dtb::next_prop(node.ptr);
		while (next_ptr) {
			u32 len = kstd::byteswap(*next_ptr++);
			u32 name_off = kstd::byteswap(*next_ptr++);

			auto* name_ptr = plain_dtb->strings + name_off;
			kstd::string_view name {name_ptr};
			if (name == "phandle" || name == "linux,phandle") {
				assert(len == 4);
				u32 handle = kstd::byteswap(*next_ptr);
				DTB->handle_map.insert(handle, new_node);
			}
			else if (name == "#size-cells") {
				assert(len == 4);
				new_node->size_cells = kstd::byteswap(*next_ptr);
			}
			else if (name == "#address-cells") {
				assert(len == 4);
				new_node->addr_cells = kstd::byteswap(*next_ptr);
			}
			else if (name == "#interrupt-cells") {
				assert(len == 4);
				new_node->interrupt_cells = kstd::byteswap(*next_ptr);
			}
			else if (name == "interrupt-controller") {
				new_node->interrupt_controller = true;
			}
			else if (name == "interrupt-parent") {
				assert(len == 4);
				new_node->interrupt_parent_handle = kstd::byteswap(*next_ptr);
				has_interrupt_parent = true;
			}
			else if (name == "compatible") {
				kstd::string_view whole_compatible {reinterpret_cast<const char*>(next_ptr), len - 1};

				size_t pos = 0;
				while (pos < whole_compatible.size()) {
					auto compatible_end = whole_compatible.find(0, pos);
					kstd::string_view compatible;
					if (compatible_end == kstd::string_view::npos) {
						compatible = whole_compatible.substr(pos);
						pos = whole_compatible.size();
					}
					else {
						compatible = whole_compatible.substr(pos, compatible_end - pos);
						pos = compatible_end + 1;
					}
					new_node->compatible.push(compatible);
				}
			}
			else if (name == "reg") {
				u32 total_cells = parent->addr_cells + parent->size_cells;

				u32 index = 0;
				while (index + total_cells <= len / 4) {
					auto read_cells = [&](u32 cells, u32& index) {
						if (cells == 2) {
							u32 first = next_ptr[index];
							u64 second = next_ptr[index + 1];
							u64 final = first | second << 32;
							index += 2;
							return kstd::byteswap(final);
						}
						else {
							return static_cast<u64>(kstd::byteswap(next_ptr[index++]));
						}
					};

					u64 addr = read_cells(parent->addr_cells, index);

					u64 size;
					if (parent->size_cells == 0) {
						size = 0;
					}
					else {
						size = read_cells(parent->size_cells, index);
					}

					new_node->regs.push({
						.addr = addr,
						.size = size
					});
				}
			}
			else if (name == "reg-names") {
				kstd::string_view whole_reg_names {reinterpret_cast<const char*>(next_ptr), len - 1};

				size_t pos = 0;
				while (pos < whole_reg_names.size()) {
					auto reg_name_end = whole_reg_names.find(0, pos);
					kstd::string_view reg_name;
					if (reg_name_end == kstd::string_view::npos) {
						reg_name = whole_reg_names.substr(pos);
						pos = whole_reg_names.size();
					}
					else {
						reg_name = whole_reg_names.substr(pos, reg_name_end - pos);
						pos = reg_name_end + 1;
					}
					new_node->reg_names.push(reg_name);
				}
			}

			new_node->properties.push({
				.name = name,
				.ptr = next_ptr,
				.size = len
			});

			next_ptr += (len + 3) / 4;
			next_ptr = dtb::Dtb::next_prop(next_ptr);
		}

		if (!has_interrupt_parent) {
			new_node->interrupt_parent_handle = new_node->parent->interrupt_parent_handle;
		}

		for (auto child : node.child_iter()) {
			traverse_stack.push({new_node, child});
		}
	}

	DTB->traverse(
		[](DtbNode&) {return true;},
		[](DtbNode& node) {
			if (auto ranges_opt = node.prop("ranges")) {
				auto ranges = ranges_opt.value();

				auto child_addr_cells = node.addr_cells;
				auto parent_addr_cells = node.parent->addr_cells;
				auto size_cells = node.size_cells;

				size_t pos = 0;
				while (pos < ranges.size) {
					DtbNode::AddrTranslation translation {};
					if (child_addr_cells == 3) {
						translation.child_addr_high = ranges.read<u32>(pos);
						pos += 4;
						translation.child_addr = ranges.read<u64>(pos);
						pos += 8;
					} else {
						assert(child_addr_cells < 3);
						translation.child_addr = ranges.read_cells(pos, child_addr_cells);
						pos += child_addr_cells * 4;
					}

					assert(parent_addr_cells < 3);
					translation.parent_addr = ranges.read_cells(pos, parent_addr_cells);
					pos += parent_addr_cells * 4;

					assert(size_cells <= 2);
					translation.size = ranges.read_cells(pos, size_cells);
					pos += size_cells * 4;

					node.addr_translations.push(translation);
				}
			}

			return false;
		});

	DTB->traverse(
		[](DtbNode&) {return true;},
		[](DtbNode& node) {
			for (auto& reg : node.regs) {
				reg.addr = node.translate_addr(reg.addr);
			}

			auto irq_controller_ptr = DTB->handle_map.get(node.interrupt_parent_handle);
			assert(irq_controller_ptr);
			auto irq_controller = *irq_controller_ptr;
			if (irq_controller->interrupt_controller) {
				node.interrupt_parent = irq_controller;

				auto irqs_opt = node.prop("interrupts");
				if (irqs_opt && irq_controller->is_compatible("arm,gic-v3")) {
					auto irq_cells = irq_controller->interrupt_cells;
					assert(irq_cells >= 3);
					auto irqs_prop = irqs_opt.value();

					u32 offset = 0;
					while (offset + irq_cells * 4 <= irqs_prop.size) {
						u32 is_ppi = irqs_prop.read<u32>(offset);
						u32 num_for_type = irqs_prop.read<u32>(offset + 4);
						u32 flags = irqs_prop.read<u32>(offset + 8);

						DtbNode::Irq irq {
							.num = num_for_type,
							.mode = TriggerMode::None
						};

						if (is_ppi) {
							irq.num += 16;
						}
						else {
							irq.num += 32;
						}

						flags &= 0xF;
						if (flags == 1 || flags == 2) {
							// flags 1 == active high edge triggered
							// flags 2 == active low edge triggered
							irq.mode = TriggerMode::Edge;
						}
						else if (flags == 4 || flags == 8) {
							// flags 4 == active high level triggered
							// flags 8 == active low level triggered
							irq.mode = TriggerMode::Level;
						}
						else {
							//println("[kernel][aarch64]: unsupported irq mode ", flags);
						}

						node.irqs.push(irq);

						offset += irq_cells * 4;
					}
				}
			}

			return false;
		});
}

u64 DtbNode::translate_addr(u64 addr) const {
	for (auto* node = parent; node; node = node->parent) {
		if (node->addr_translations.is_empty()) {
			continue;
		}

		if (!node->is_compatible("simple-bus")) {
			//println("[kernel][aarch64]: unsupported translation on bus ", node->name);
			return addr;
		}

		for (auto& translation : node->addr_translations) {
			if (addr >= translation.child_addr &&
				addr < translation.child_addr + translation.size) {
				addr -= translation.child_addr;
				addr += translation.parent_addr;
				break;
			}
		}
	}

	return addr;
}
