#pragma once
#include "string_view.hpp"
#include "vector.hpp"
#include "unordered_map.hpp"
#include "concepts.hpp"
#include "manually_init.hpp"
#include "optional.hpp"
#include "bit.hpp"
#include "dev/gic.hpp"

struct DtbProperty {
	kstd::string_view name {};

	[[nodiscard]] kstd::string_view as_string() const {
		return kstd::string_view {reinterpret_cast<const char*>(ptr), size - 1};
	}

	template<kstd::integral T>
	[[nodiscard]] inline T read(u32 offset) const {
		assert(offset + sizeof(T) <= size);

		if constexpr (sizeof(T) == 8) {
			u32 first = *offset(ptr, const u32*, offset);
			u64 second = *offset(ptr, const u32*, offset + 4);
			u64 final = first | second << 32;
			return kstd::byteswap(final);
		}
		else {
			return kstd::byteswap(*offset(ptr, const T*, offset));
		}
	}

	[[nodiscard]] inline u64 read_cells(u32 offset, u32 cells) const {
		assert(offset + cells * 4 <= size);
		if (cells == 1) {
			return read<u32>(offset);
		}
		else if (cells == 2) {
			return read<u64>(offset);
		}
		else {
			__builtin_trap();
		}
	}

	u32* ptr;
	u32 size;
};

struct DtbNode {
	kstd::string_view name {};
	DtbNode* parent {};
	DtbNode* interrupt_parent {};
	kstd::vector<DtbNode*> children;
	kstd::vector<kstd::string_view> compatible;
	kstd::vector<DtbProperty> properties;

	struct Reg {
		u64 addr;
		u64 size;
	};
	kstd::vector<Reg> regs {};

	kstd::optional<Reg> reg_by_name(kstd::string_view reg_name) {
		for (usize i = 0; i < reg_names.size(); ++i) {
			if (reg_names[i] == reg_name) {
				assert(i < regs.size());
				return regs[i];
			}
		}
		return {};
	}

	kstd::optional<Reg> reg_by_name_fallback(kstd::string_view reg_name, usize fallback_index) {
		if (auto reg = reg_by_name(reg_name)) {
			return reg;
		}
		else {
			assert(fallback_index < regs.size());
			return regs[fallback_index];
		}
	}

	struct Irq {
		u32 num;
		TriggerMode mode;
	};
	kstd::vector<Irq> irqs {};
	void* data {};

	inline bool is_compatible(kstd::string_view comp) {
		for (const auto& i : compatible) {
			if (i == comp) {
				return true;
			}
		}
		return false;
	}

	inline kstd::optional<DtbProperty> prop(kstd::string_view prop_name) {
		for (auto& prop : properties) {
			if (prop.name == prop_name) {
				return prop;
			}
		}
		return {};
	}

	[[nodiscard]] u64 translate_addr(u64 addr) const;

private:
	struct AddrTranslation {
		u64 child_addr;
		u32 child_addr_high;
		u64 parent_addr;
		u64 size;
	};

	u32 addr_cells {};
	u32 size_cells {};
	u32 interrupt_cells {};
	u32 interrupt_parent_handle {};
	kstd::vector<AddrTranslation> addr_translations;
	kstd::vector<kstd::string_view> reg_names {};
	bool interrupt_controller {};

	friend void kernel_dtb_init(void* plain_dtb);
};

struct Dtb {
	DtbNode* root {};
	kstd::unordered_map<DtbNode*, u32> handle_map;

	template<typename C, typename F> requires requires(C c, F f, DtbNode& node) {
		{c(node)} -> kstd::same_as<bool>;
		{f(node)} -> kstd::same_as<bool>;
	}
	void traverse(C condition, F fn) {
		kstd::vector<DtbNode*> stack;
		stack.push(root);

		while (!stack.is_empty()) {
			auto node = stack.pop().value();

			if (condition(*node)) {
				if (fn(*node)) {
					break;
				}
			}

			for (auto child : node->children) {
				stack.push(child);
			}
		}
	}
};

extern ManuallyInit<Dtb> DTB;
