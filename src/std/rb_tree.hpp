#pragma once
#include "assert.hpp"

struct RbTreeHook {
	void* parent;
	void* child[2];
	void* predecessor;
	void* successor;
	int color;
};

template<typename T, RbTreeHook (T::*Hook)>
class RbTreeBase {
public:
	static constexpr T* get_left(T* node) {
		return static_cast<T*>((node->*Hook).child[0]);
	}

	static constexpr T* get_right(T* node) {
		return static_cast<T*>((node->*Hook).child[1]);
	}

	static constexpr T* get_parent(T* node) {
		return static_cast<T*>((node->*Hook).parent);
	}

	static constexpr T* get_successor(T* node) {
		return static_cast<T*>((node->*Hook).successor);
	}

	static constexpr T* get_predecessor(T* node) {
		return static_cast<T*>((node->*Hook).predecessor);
	}

	constexpr T* get_root() {
		return root;
	}
	constexpr const T* get_root() const {
		return root;
	}

	constexpr T* get_first() {
		T* node = root;
		if (!node) {
			return nullptr;
		}
		while ((node->*Hook).child[0]) {
			node = static_cast<T*>((node->*Hook).child[0]);
		}
		return node;
	}

	constexpr T* get_last() {
		T* node = root;
		if (!node) {
			return nullptr;
		}
		while ((node->*Hook).child[1]) {
			node = static_cast<T*>((node->*Hook).child[1]);
		}
		return node;
	}

	constexpr void remove(T* node) {
		T* left = static_cast<T*>((node->*Hook).child[0]);
		T* right = static_cast<T*>((node->*Hook).child[1]);

		if (!left) {
			remove_half_leaf(node, right);
		}
		else if (!right) {
			remove_half_leaf(node, left);
		}
		else {
			T* predecessor = static_cast<T*>((node->*Hook).predecessor);
			remove_half_leaf(predecessor, static_cast<T*>((predecessor->*Hook).child[0]));
			replace_node(node, predecessor);
		}
	}

	constexpr void insert_root(T* node) {
		(node->*Hook) = {};

		assert(!root);
		root = node;
		fix_insert(node);
	}

	constexpr void insert_left(T* parent, T* node) {
		(node->*Hook) = {};

		assert(parent);
		assert(!(parent->*Hook).child[0]);
		(parent->*Hook).child[0] = node;
		(node->*Hook).parent = parent;

		T* predecessor = static_cast<T*>((parent->*Hook).predecessor);
		if (predecessor) {
			(predecessor->*Hook).successor = node;
		}
		(node->*Hook).predecessor = predecessor;
		(node->*Hook).successor = parent;
		(parent->*Hook).predecessor = node;
		fix_insert(node);
	}

	constexpr void insert_right(T* parent, T* node) {
		(node->*Hook) = {};

		assert(parent);
		assert(!(parent->*Hook).child[1]);
		(parent->*Hook).child[1] = node;
		(node->*Hook).parent = parent;

		T* successor = static_cast<T*>((parent->*Hook).successor);
		if (successor) {
			(successor->*Hook).predecessor = node;
		}
		(node->*Hook).successor = successor;
		(node->*Hook).predecessor = parent;
		(parent->*Hook).successor = node;
		fix_insert(node);
	}

private:
	T* root;

	static constexpr bool is_red(T* node) {
		return node && (node->*Hook).color == 1;
	}

	static constexpr bool is_black(T* node) {
		return !node || (node->*Hook).color == 0;
	}

	constexpr void rotate(T* node, bool right) {
		T* parent = static_cast<T*>((node->*Hook).parent);
		assert(parent && (parent->*Hook).child[!right] == node);
		T* child = static_cast<T*>((node->*Hook).child[right]);
		T* grandparent = static_cast<T*>((parent->*Hook).parent);

		if (child) {
			(child->*Hook).parent = parent;
		}
		(parent->*Hook).child[!right] = child;
		(parent->*Hook).parent = node;
		(node->*Hook).child[right] = parent;
		(node->*Hook).parent = grandparent;

		if (!grandparent) {
			root = node;
		}
		else if ((grandparent->*Hook).child[0] == parent) {
			(grandparent->*Hook).child[0] = node;
		}
		else {
			assert((grandparent->*Hook).child[1] == parent);
			(grandparent->*Hook).child[1] = node;
		}
	}

	constexpr void fix_insert(T* node) {
		while (true) {
			T* parent = static_cast<T*>((node->*Hook).parent);
			if (!parent) {
				(node->*Hook).color = 0;
				return;
			}

			(node->*Hook).color = 1;
			if ((parent->*Hook).color == 0) {
				return;
			}

			T* grandparent = static_cast<T*>((parent->*Hook).parent);
			assert(grandparent && (grandparent->*Hook).color == 0);

			if ((grandparent->*Hook).child[0] == parent
				&& is_red(static_cast<T*>((grandparent->*Hook).child[1]))) {
				(grandparent->*Hook).color = 1;
				(parent->*Hook).color = 0;
				(static_cast<T*>((grandparent->*Hook).child[1])->*Hook).color = 0;
				node = grandparent;
				continue;
			}
			else if ((grandparent->*Hook).child[1] == parent
				&& is_red(static_cast<T*>((grandparent->*Hook).child[0]))) {
				(grandparent->*Hook).color = 1;
				(parent->*Hook).color = 0;
				(static_cast<T*>((grandparent->*Hook).child[0])->*Hook).color = 0;
				node = grandparent;
				continue;
			}

			if ((grandparent->*Hook).child[0] == parent) {
				if ((parent->*Hook).child[1] == node) {
					rotate(node, false);
					rotate(node, true);
					(node->*Hook).color = 0;
				}
				else {
					rotate(parent, true);
					(parent->*Hook).color = 0;
				}
				(grandparent->*Hook).color = 1;
			}
			else {
				assert((grandparent->*Hook).child[1] == parent);
				if ((parent->*Hook).child[0] == node) {
					rotate(node, true);
					rotate(node, false);
					(node->*Hook).color = 0;
				}
				else {
					rotate(parent, false);
					(parent->*Hook).color = 0;
				}
				(grandparent->*Hook).color = 1;
			}

			return;
		}
	}

	constexpr void fix_remove(T* node) {
		while (true) {
			assert((node->*Hook).color == 0);

			T* parent = static_cast<T*>((node->*Hook).parent);
			if (!parent) {
				return;
			}

			bool node_is_left = (parent->*Hook).child[0] == node;
			assert((parent->*Hook).child[node_is_left]);
			if (auto n = static_cast<T*>((parent->*Hook).child[node_is_left]);
				(n->*Hook).color == 1) {
				rotate(n, !node_is_left);
				assert(node == (parent->*Hook).child[!node_is_left]);
				(parent->*Hook).color = 1;
				(n->*Hook).color = 0;
			}
			T* sibling = static_cast<T*>((parent->*Hook).child[node_is_left]);

			if (is_black(static_cast<T*>((sibling->*Hook).child[0]))
				&& is_black(static_cast<T*>((sibling->*Hook).child[1]))) {

				(sibling->*Hook).color = 1;
				if ((parent->*Hook).color == 0) {
					node = parent;
					continue;
				}
				else {
					(parent->*Hook).color = 0;
					return;
				}
			}

			int parent_color = (parent->*Hook).color;
			node_is_left = (parent->*Hook).child[0] == node;
			if (auto child = static_cast<T*>((sibling->*Hook).child[!node_is_left]);
				is_red(child) && is_black(static_cast<T*>((sibling->*Hook).child[node_is_left]))) {
				rotate(child, node_is_left);
				(sibling->*Hook).color = 1;
				(child->*Hook).color = 0;
				sibling = child;
			}
			assert(is_red(static_cast<T*>((sibling->*Hook).child[node_is_left])));
			rotate(sibling, !node_is_left);
			(parent->*Hook).color = 0;
			(sibling->*Hook).color = parent_color;
			(static_cast<T*>((sibling->*Hook).child[node_is_left])->*Hook).color = 0;

			return;
		}
	}

	constexpr void replace_node(T* node, T* replacement) {
		T* parent = static_cast<T*>((node->*Hook).parent);
		T* left = static_cast<T*>((node->*Hook).child[0]);
		T* right = static_cast<T*>((node->*Hook).child[1]);

		if (!parent) {
			root = replacement;
		}
		else if ((parent->*Hook).child[0] == node) {
			(parent->*Hook).child[0] = replacement;
		}
		else {
			assert((parent->*Hook).child[1] == node);
			(parent->*Hook).child[1] = replacement;
		}
		(replacement->*Hook).parent = parent;
		(replacement->*Hook).color = (node->*Hook).color;
		(replacement->*Hook).child[0] = left;
		(replacement->*Hook).child[1] = right;
		if (left) {
			(left->*Hook).parent = replacement;
		}
		if (right) {
			(right->*Hook).parent = replacement;
		}

		if (auto predecessor = static_cast<T*>((node->*Hook).predecessor)) {
			(predecessor->*Hook).successor = replacement;
		}
		(replacement->*Hook).predecessor = (node->*Hook).predecessor;
		(replacement->*Hook).successor = (node->*Hook).successor;
		if (auto successor = static_cast<T*>((node->*Hook).successor)) {
			(successor->*Hook).predecessor = replacement;
		}

		(node->*Hook).child[0] = nullptr;
		(node->*Hook).child[1] = nullptr;
		(node->*Hook).parent = nullptr;
		(node->*Hook).predecessor = nullptr;
		(node->*Hook).successor = nullptr;
	}

	constexpr void remove_half_leaf(T* node, T* child) {
		T* predecessor = static_cast<T*>((node->*Hook).predecessor);
		T* successor = static_cast<T*>((node->*Hook).successor);
		if (predecessor) {
			(predecessor->*Hook).successor = successor;
		}
		if (successor) {
			(successor->*Hook).predecessor = predecessor;
		}

		if ((node->*Hook).color == 0) {
			if (is_red(child)) {
				(child->*Hook).color = 0;
			}
			else {
				fix_remove(node);
			}
		}

		assert((!(node->*Hook).child[0] && (node->*Hook).child[1] == child)
			|| (!(node->*Hook).child[1] && (node->*Hook).child[0] == child));

		T* parent = static_cast<T*>((node->*Hook).parent);
		if (!parent) {
			root = child;
		}
		else if ((parent->*Hook).child[0] == node) {
			(parent->*Hook).child[0] = child;
		}
		else {
			assert((parent->*Hook).child[1] == node);
			(parent->*Hook).child[1] = child;
		}

		if (child) {
			(child->*Hook).parent = parent;
		}

		(node->*Hook).child[0] = nullptr;
		(node->*Hook).child[1] = nullptr;
		(node->*Hook).parent = nullptr;
		(node->*Hook).predecessor = nullptr;
		(node->*Hook).successor = nullptr;
	}
};

template<typename T, RbTreeHook (T::*Hook)> requires requires(T a, T b) {
	a <=> b;
}
class RbTree : public RbTreeBase<T, Hook> {
public:
	constexpr void insert(T* node) {
		auto* n = this->get_root();

		if (!n) {
			this->insert_root(node);
			return;
		}

		T* prev;
		while (n) {
			prev = n;
			auto ret = *node <=> *n;
			if (ret < 0) {
				n = RbTreeBase<T, Hook>::get_left(n);
			}
			else if (ret > 0) {
				n = RbTreeBase<T, Hook>::get_right(n);
			}
			else {
				assert(false && "tried to insert duplicate key");
			}
		}

		auto ret = *node <=> *prev;
		if (ret < 0) {
			this->insert_left(prev, node);
		}
		else {
			this->insert_right(prev, node);
		}
	}

	template<typename K, K (T::*Key)> requires requires(K lhs, K rhs) {
		lhs < rhs;
		lhs > rhs;
		lhs == rhs;
	}
	constexpr T* find(K key) {
		auto* n = this->get_root();

		if (!n) {
			return nullptr;
		}

		while (n) {
			if (key < (n->*Key)) {
				n = RbTreeBase<T, Hook>::get_left(n);
			}
			else if (key > (n->*Key)) {
				n = RbTreeBase<T, Hook>::get_right(n);
			}
			else {
				return n;
			}
		}

		return nullptr;
	}
};
