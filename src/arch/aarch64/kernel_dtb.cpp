#include "kernel_dtb.hpp"
#include "dtb.hpp"

ManuallyInit<Dtb> DTB {};

void kernel_dtb_init(dtb::Dtb plain_dtb) {
	DTB.initialize();

	auto root = plain_dtb.get_root();

	kstd::vector<kstd::pair<DtbNode*, dtb::Node>> traverse_stack {};
	traverse_stack.push({nullptr, root});

	while (!traverse_stack.is_empty()) {
		auto [parent, node] = traverse_stack.pop().value();

		println("creating node ", node.name);

		auto* new_node = new DtbNode {
			.name = node.name,
			.parent = parent,
			.children {},
			.compatible {},
			.cells {}
		};
		if (parent) {
			parent->children.push(new_node);
		}
		else {
			DTB->root = new_node;
		}

		auto* next_ptr = dtb::Dtb::next_prop(node.ptr);
		while (next_ptr) {
			u32 len = kstd::byteswap(*next_ptr++);
			u32 name_off = kstd::byteswap(*next_ptr++);

			auto* name_ptr = plain_dtb.strings + name_off;
			kstd::string_view name {name_ptr};
			if (name == "phandle" || name == "linux,phandle") {
				assert(len == 4);
				u32 handle = kstd::byteswap(*next_ptr);
				DTB->handle_map.insert(handle, new_node);
			}
			else if (name == "#size-cells") {
				assert(len == 4);
				new_node->cells.size = kstd::byteswap(*next_ptr);
			}
			else if (name == "#address-cells") {
				assert(len == 4);
				new_node->cells.addr = kstd::byteswap(*next_ptr);
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

			next_ptr += (len + 3) / 4;
			next_ptr = dtb::Dtb::next_prop(next_ptr);
		}

		for (auto child : node.child_iter()) {
			traverse_stack.push({new_node, child});
		}
	}
}
