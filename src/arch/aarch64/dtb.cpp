#include <cstring.hpp>
#include "dtb.hpp"

constexpr u32 FDT_BEGIN_NODE = 0x1;
constexpr u32 FDT_END_NODE = 0x2;
constexpr u32 FDT_PROP = 0x3;
constexpr u32 FDT_NOP = 0x4;
constexpr u32 FDT_END = 0x9;

namespace dtb {
	Dtb::Dtb(void* ptr) {
		auto hdr = dtb::FdtHeader::parse(static_cast<u32*>(ptr));
		strings = offset(ptr, const char*, hdr.off_dt_strings);
		root_ptr = offset(ptr, u32*, hdr.off_dt_struct);
		reserved = offset(ptr, FdtReserveEntry*, hdr.off_mem_rsvmap);
		auto r = get_root();

		if (auto p = r.prop("#size-cells")) {
			size_cells.size_cells = p.value().read<u32>(0);
		}
		if (auto p = r.prop("#address-cells")) {
			size_cells.addr_cells = p.value().read<u32>(0);
		}
		total_size = hdr.total_size;
	}

	Node Dtb::get_root() {
		return {this, root_ptr};
	}

	Dtb::ReservedEntryIterator Dtb::get_reserved() {
		return ReservedEntryIterator {reserved};
	}

	u32* Dtb::next(u32* ptr) {
		u32 depth = 0;
		while (true) {
			u32 token = kstd::byteswap(*ptr++);
			if (token == FDT_BEGIN_NODE) {
				++depth;
				auto* name = reinterpret_cast<const char*>(ptr);
				ptr += (strlen(name) + 4) / 4;
			}
			else if (token == FDT_PROP) {
				u32 len = kstd::byteswap(*ptr++);
				// name_off
				++ptr;
				ptr += (len + 3) / 4;
			}
			else if (token == FDT_END_NODE) {
				if (depth == 0) {
					while (kstd::byteswap(*ptr) == FDT_NOP) {
						++ptr;
					}
					if (kstd::byteswap(*ptr) != FDT_BEGIN_NODE) {
						return nullptr;
					}
					return ptr;
				}
				else {
					--depth;
				}
			}
			else if (token == FDT_END) {
				return nullptr;
			}
		}
	}

	u32* Dtb::next_child(u32* ptr) {
		while (true) {
			u32 token = kstd::byteswap(*ptr++);
			if (token == FDT_BEGIN_NODE) {
				return ptr - 1;
			}
			else if (token == FDT_PROP) {
				u32 len = kstd::byteswap(*ptr++);
				// name_off
				++ptr;
				ptr += (len + 3) / 4;
			}
			else if (token == FDT_END || token == FDT_END_NODE) {
				return nullptr;
			}
		}
	}

	u32* Dtb::next_prop(u32* ptr) {
		u32 depth = 0;
		while (true) {
			u32 token = kstd::byteswap(*ptr++);
			if (token == FDT_BEGIN_NODE) {
				++depth;
				auto n = reinterpret_cast<const char*>(ptr);
				ptr += (strlen(n) + 3) / 4;
			}
			else if (token == FDT_PROP) {
				if (depth == 0) {
					return ptr;
				}

				u32 len = kstd::byteswap(*ptr++);
				// name_off
				++ptr;
				ptr += (len + 3) / 4;
			}
			else if (token == FDT_END_NODE) {
				if (depth == 0) {
					return nullptr;
				}
				--depth;
			}
			else if (token == FDT_END) {
				return nullptr;
			}
		}
	}

	Node::Node(const Dtb* dtb, u32* start) : dtb {dtb} {
		++start;
		auto* n = reinterpret_cast<const char*>(start);
		u32 len = strlen(n);
		name = {n, len};
		auto name_end = name.find('@');
		if (name_end != kstd::string_view::npos) {
			unit = name.substr(name_end + 1);
			name = name.substr(0, name_end);
		}
		ptr = start + (len + 1 + 3) / 4;
	}

	kstd::optional<Prop> Node::prop(kstd::string_view prop_name) const {
		auto* next_ptr = Dtb::next_prop(ptr);
		while (next_ptr) {
			u32 len = kstd::byteswap(*next_ptr++);
			u32 name_off = kstd::byteswap(*next_ptr++);

			auto* name_ptr = dtb->strings + name_off;
			if (name_ptr == prop_name) {
				return Prop {.ptr = next_ptr, .len = len};
			}

			next_ptr += (len + 3) / 4;
			next_ptr = Dtb::next_prop(next_ptr);
		}
		return {};
	}

	kstd::optional<Node> Node::next() const {
		auto* next_ptr = Dtb::next(ptr);
		if (!next_ptr) {
			return {};
		}
		return Node {dtb, next_ptr};
	}

	kstd::optional<Node> Node::child() const {
		auto* next_ptr = Dtb::next_child(ptr);
		if (!next_ptr) {
			return {};
		}
		return Node {dtb, next_ptr};
	}

	kstd::optional<Node> Node::find_child(kstd::string_view child_name) const {
		auto node = child();
		while (node) {
			auto n = node.value();
			if (n.name == child_name) {
				return n;
			}
			node = n.next();
		}
		return {};
	}

	kstd::optional<kstd::string_view> Node::compatible() const {
		auto p = prop("compatible");
		if (!p) {
			return {};
		}
		auto pr = p.value();
		return kstd::string_view {static_cast<const char*>(pr.ptr), pr.len - 1};
	}

	kstd::optional<Node::Reg> Node::reg(const SizeAddrCells& cells, u32 index) const {
		auto p_opt = prop("reg");
		if (!p_opt) {
			return {};
		}
		auto p = p_opt.value();
		u32 addr_size = cells.addr_cells * 4;
		u32 total_size = addr_size + cells.size_cells * 4;
		if (index * total_size >= p.len) {
			return {};
		}

		u64 addr;
		if (cells.addr_cells == 1) {
			addr = p.read<u32>(index * total_size);
		}
		else if (cells.addr_cells == 2) {
			addr = p.read<u64>(index * total_size);
		}
		else {
			__builtin_trap();
		}

		u64 size;
		if (cells.size_cells == 0) {
			size = 0;
		}
		else if (cells.size_cells == 1) {
			size = p.read<u32>(index * total_size + addr_size);
		}
		else if (cells.size_cells == 2) {
			size = p.read<u64>(index * total_size + addr_size);
		}
		else {
			__builtin_trap();
		}

		return Reg {.addr = addr, .size = size};
	}

	SizeAddrCells Node::size_cells() const {
		SizeAddrCells cells = dtb->size_cells;
		if (auto p = prop("#size-cells")) {
			cells.size_cells = p.value().read<u32>(0);
		}
		if (auto p = prop("#address-cells")) {
			cells.addr_cells = p.value().read<u32>(0);
		}

		return cells;
	}

	Node::ChildIterator<Node> Node::child_iter() {
		return ChildIterator<Node> {*this};
	}
}
