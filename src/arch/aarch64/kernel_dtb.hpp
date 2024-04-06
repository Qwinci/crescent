#pragma once
#include "string_view.hpp"
#include "vector.hpp"
#include "unordered_map.hpp"
#include "concepts.hpp"
#include "manually_init.hpp"

struct DtbCells {
	u32 addr;
	u32 size;
};

struct DtbNode {
	kstd::string_view name;
	DtbNode* parent;
	kstd::vector<DtbNode*> children;
	kstd::vector<kstd::string_view> compatible;
	DtbCells cells {2, 1};

	inline bool is_compatible(kstd::string_view comp) {
		for (const auto& i : compatible) {
			if (i == comp) {
				return true;
			}
		}
		return false;
	}
};

struct Dtb {
	DtbNode* root {};
	kstd::unordered_map<DtbNode*, u32> handle_map;

	template<typename C, typename F> requires requires(C c, F f, DtbNode& node) {
		{c(node)} -> kstd::same_as<bool>;
		{f(node)} -> kstd::same_as<void>;
	}
	void traverse(C condition, F fn) {
		kstd::vector<DtbNode*> stack;
		stack.push(root);

		while (!stack.is_empty()) {
			auto node = stack.pop().value();

			if (condition(*node)) {
				fn(*node);
				break;
			}

			for (auto child : node->children) {
				stack.push(child);
			}
		}
	}
};

extern ManuallyInit<Dtb> DTB;
