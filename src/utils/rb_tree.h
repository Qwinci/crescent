#pragma once

typedef struct RbTreeNode {
	struct RbTreeNode* parent;
	struct RbTreeNode* left;
	struct RbTreeNode* right;
	struct RbTreeNode* predecessor;
	struct RbTreeNode* successor;
	int color;
} RbTreeNode;

typedef struct {
	RbTreeNode* root;
} RbTree;

RbTreeNode* rb_tree_get_first(const RbTree* self);
RbTreeNode* rb_tree_get_last(const RbTree* self);
void rb_tree_remove(RbTree* self, RbTreeNode* node);
void rb_tree_insert_root(RbTree* self, RbTreeNode* node);
void rb_tree_insert_left(RbTree* self, RbTreeNode* parent, RbTreeNode* node);
void rb_tree_insert_right(RbTree* self, RbTreeNode* parent, RbTreeNode* node);
