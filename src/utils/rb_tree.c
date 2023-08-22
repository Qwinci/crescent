#include "rb_tree.h"
#if __STDC_VERSION__ < 202000L || (!defined(__clang__) && __GNUC__ < 13)
#include <stdbool.h>
#endif
#include <stddef.h>

#define HOSTED 1
#if HOSTED == 1
#include <assert.h>
#else
#define assert(...)
#endif

static bool is_red(RbTreeNode* self) {
	return self && self->color == 1;
}

static bool is_black(RbTreeNode* self) {
	return !self || self->color == 0;
}

static void rotate_left(RbTree* self, RbTreeNode* node) {
	RbTreeNode* parent = node->parent;
	assert(parent && parent->right == node);
	RbTreeNode* left = node->left;
	RbTreeNode* grandparent = parent->parent;

	if (left) {
		left->parent = parent;
	}
	parent->right = left;
	parent->parent = node;
	node->left = parent;
	node->parent = grandparent;

	if (!grandparent) {
		self->root = node;
	}
	else if (grandparent->left == parent) {
		grandparent->left = node;
	}
	else {
		assert(grandparent->right == parent);
		grandparent->right = node;
	}
}

static void rotate_right(RbTree* self, RbTreeNode* node) {
	RbTreeNode* parent = node->parent;
	assert(parent && parent->left == node);
	RbTreeNode* right = node->right;
	RbTreeNode* grandparent = parent->parent;

	if (right) {
		right->parent = parent;
	}
	parent->left = right;
	parent->parent = node;
	node->right = parent;
	node->parent = grandparent;

	if (!grandparent) {
		self->root = node;
	}
	else if (grandparent->left == parent) {
		grandparent->left = node;
	}
	else {
		assert(grandparent->right == parent);
		grandparent->right = node;
	}
}

static void fix_insert(RbTree* self, RbTreeNode* node) {
	RbTreeNode* parent = node->parent;
	if (!parent) {
		node->color = 0;
		return;
	}

	node->color = 1;
	if (parent->color == 0) {
		return;
	}

	RbTreeNode* grandparent = parent->parent;
	assert(grandparent && grandparent->color == 0);

	if (grandparent->left == parent && is_red(grandparent->right)) {
		grandparent->color = 1;
		parent->color = 0;
		grandparent->right->color = 0;
		fix_insert(self, grandparent);
		return;
	}
	else if (grandparent->right == parent && is_red(grandparent->left)) {
		grandparent->color = 1;
		parent->color = 0;
		grandparent->left->color = 0;
		fix_insert(self, grandparent);
		return;
	}

	if (parent == grandparent->left) {
		if (node == parent->right) {
			rotate_left(self, node);
			rotate_right(self, node);
			node->color = 0;
		}
		else {
			rotate_right(self, parent);
			parent->color = 0;
		}
		grandparent->color = 1;
	}
	else {
		assert(parent == grandparent->right);
		if (node == parent->left) {
			rotate_right(self, node);
			rotate_left(self, node);
			node->color = 0;
		}
		else {
			rotate_left(self, parent);
			parent->color = 0;
		}
		grandparent->color = 1;
	}
}

static void fix_remove(RbTree* self, RbTreeNode* node) {
	assert(node->color == 0);

	RbTreeNode* parent = node->parent;
	if (!parent) {
		return;
	}

	RbTreeNode* sibling;
	if (parent->left == node) {
		assert(parent->right);
		if (parent->right->color == 1) {
			RbTreeNode* n = parent->right;
			rotate_left(self, n);
			assert(node == parent->left);

			parent->color = 1;
			n->color = 0;
		}

		sibling = parent->right;
	}
	else {
		assert(parent->right == node);
		assert(parent->left);
		if (parent->left->color == 1) {
			RbTreeNode* n = parent->left;
			rotate_right(self, n);
			assert(node == parent->right);

			parent->color = 1;
			n->color = 0;
		}

		sibling = parent->left;
	}

	if (is_black(sibling->left) && is_black(sibling->right)) {
		if (parent->color == 0) {
			sibling->color = 1;
			fix_remove(self, parent);
			return;
		}
		else {
			parent->color = 0;
			sibling->color = 1;
			return;
		}
	}

	int parent_color = parent->color;
	if (parent->left == node) {
		if (is_red(sibling->left) && is_black(sibling->right)) {
			RbTreeNode* child = sibling->left;
			rotate_right(self, child);

			sibling->color = 1;
			child->color = 0;

			sibling = child;
		}
		assert(is_red(sibling->right));

		rotate_left(self, sibling);
		parent->color = 0;
		sibling->color = parent_color;
		sibling->right->color = 0;
	}
	else {
		assert(parent->right == node);

		if (is_red(sibling->right) && is_black(sibling->left)) {
			RbTreeNode* child = sibling->right;
			rotate_left(self, child);

			sibling->color = 1;
			child->color = 0;

			sibling = child;
		}
		assert(is_red(sibling->left));

		rotate_right(self, sibling);
		parent->color = 0;
		sibling->color = parent_color;
		sibling->left->color = 0;
	}
}

static void replace_node(RbTree* self, RbTreeNode* node, RbTreeNode* replacement) {
	RbTreeNode* parent = node->parent;
	RbTreeNode* left = node->left;
	RbTreeNode* right = node->right;

	if (!parent) {
		self->root = replacement;
	}
	else if (node == parent->left) {
		parent->left = replacement;
	}
	else {
		assert(node == parent->right);
		parent->right = replacement;
	}
	replacement->parent = parent;
	replacement->color = node->color;

	replacement->left = left;
	replacement->right = right;
	if (left) {
		left->parent = replacement;
	}
	if (right) {
		right->parent = replacement;
	}

	if (node->predecessor) {
		node->predecessor->successor = replacement;
	}
	replacement->predecessor = node->predecessor;
	replacement->successor = node->successor;
	if (node->successor) {
		node->successor->predecessor = replacement;
	}

	node->left = NULL;
	node->right = NULL;
	node->parent = NULL;
	node->predecessor = NULL;
	node->successor = NULL;
}

static void remove_half_leaf(RbTree* self, RbTreeNode* node, RbTreeNode* child) {
	RbTreeNode* predecessor = node->predecessor;
	RbTreeNode* successor = node->successor;
	if (predecessor) {
		predecessor->successor = successor;
	}
	if (successor) {
		successor->predecessor = predecessor;
	}

	if (node->color == 0) {
		if (is_red(child)) {
			child->color = 0;
		}
		else {
			fix_remove(self, node);
		}
	}

	assert((!node->left && node->right == child) || (!node->right && node->left == child));

	RbTreeNode* parent = node->parent;
	if (!parent) {
		self->root = child;
	}
	else if (parent->left == node) {
		parent->left = child;
	}
	else {
		assert(parent->right == node);
		parent->right = child;
	}
	if (child) {
		child->parent = parent;
	}

	node->left = NULL;
	node->right = NULL;
	node->parent = NULL;
	node->predecessor = NULL;
	node->successor = NULL;
}

RbTreeNode* rb_tree_get_first(const RbTree* self) {
	RbTreeNode* node = self->root;
	if (!node) {
		return NULL;
	}
	while (node->left) {
		node = node->left;
	}
	return node;
}

RbTreeNode* rb_tree_get_last(const RbTree* self) {
	RbTreeNode* node = self->root;
	if (!node) {
		return NULL;
	}
	while (node->right) {
		node = node->right;
	}
	return node;
}

void rb_tree_remove(RbTree* self, RbTreeNode* node) {
	RbTreeNode* left = node->left;
	RbTreeNode* right = node->right;

	if (!left) {
		remove_half_leaf(self, node, right);
	}
	else if (!right) {
		remove_half_leaf(self, node, left);
	}
	else {
		RbTreeNode* predecessor = node->predecessor;
		remove_half_leaf(self, predecessor, predecessor->left);
		replace_node(self, node, predecessor);
	}
}

void rb_tree_insert_root(RbTree* self, RbTreeNode* node) {
	assert(!self->root);
	self->root = node;
	fix_insert(self, node);
}

void rb_tree_insert_left(RbTree* self, RbTreeNode* parent, RbTreeNode* node) {
	assert(parent);
	assert(!parent->left);

	parent->left = node;
	node->parent = parent;

	RbTreeNode* predecessor = parent->predecessor;
	if (predecessor) {
		predecessor->successor = node;
	}
	node->predecessor = predecessor;
	node->successor = parent;
	parent->predecessor = node;
	fix_insert(self, node);
}

void rb_tree_insert_right(RbTree* self, RbTreeNode* parent, RbTreeNode* node) {
	assert(parent);
	assert(!parent->right);

	parent->right = node;
	node->parent = parent;

	RbTreeNode* successor = parent->successor;
	if (successor) {
		successor->predecessor = node;
	}
	node->successor = successor;
	node->predecessor = parent;
	parent->successor = node;
	fix_insert(self, node);
}
