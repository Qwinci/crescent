#pragma once
#include <stdint.h>
#include "console.hpp"

template<typename K, typename T>
class RBTree {
	enum class Color : uint8_t {
		Black,
		Red
	};
public:
	struct Node {
		Node() : value {}, key {}, color {Color::Black} {}
		Node(K key, T value) : value {value}, key {key} {}

		T value;
	private:
		K key;
		Node* left = nullptr;
		Node* right = nullptr;
		Node* parent = nullptr;
		Color color = Color::Red;

		friend class RBTree;
	};

	Node* get_root() {
		if (root == &null) {
			return nullptr;
		}
		return root;
	}

	constexpr void insert(K key, T value, Node* node) {
		node->key = key;
		node->value = value;

		Node* prev = &null;
		Node* n = root;
		while (n != &null) {
			prev = n;
			if (key < n->key) n = n->left;
			else n = n->right;
		}
		node->parent = prev;
		if (prev == &null) root = node;
		else if (key < prev->key) prev->left = node;
		else prev->right = node;
		node->left = &null;
		node->right = &null;
		node->color = Color::Red;
		insertFix(node);
	}

	constexpr Node* remove(Node* node) {
		if (as<u8>(node->color) != 0 && as<u8>(node->color) != 1) {
			panic("node has invalid color");
		}

		Color original_color = node->color;
		Node* replace;
		if (node->left == &null) {
			replace = node->right;
			transplant(node, replace);
		}
		else if (node->right == &null) {
			replace = node->left;
			transplant(node, replace);
		}
		else {
			Node* successor = min(node->right);
			original_color = successor->color;
			replace = successor->right;
			if (successor->parent == node) replace->parent = successor;
			else {
				transplant(successor, replace);
				successor->right = node->right;
				successor->right->parent = successor;
			}
			transplant(node, successor);
			successor->left = node->left;
			successor->left->parent = successor;
			successor->color = node->color;
		}
		if (original_color == Color::Black) deleteFix(replace);
		return replace;
	}

	constexpr Node* search(K key) {
		Node* node = root;
		while (node != &null) {
			if (key < node->key) node = node->left;
			else if (key > node->key) node = node->right;
			else return node;
		}
		return nullptr;
	}

	constexpr Node* successor(Node* node) {
		if (node->right != &null) return min(node->right);
		Node* parent = node->parent;
		while (parent != &null && node == parent->right) {
			node = parent;
			parent = parent->parent;
		}
		return parent;
	}

	constexpr Node* predecessor(Node* node) {
		if (node->left != &null) return max(node->left);
		Node* parent = node->parent;
		while (parent != &null && node == parent->left) {
			node = parent;
			parent = parent->parent;
		}
		return parent;
	}

	[[nodiscard]] constexpr intmax_t height() const {
		return height(root);
	}

	constexpr intmax_t height(Node* node) const {
		if (node == &null) return -1;
		intmax_t left_height = height(node->left);
		intmax_t right_height = height(node->right);
		return (left_height > right_height ? left_height : right_height) + 1;
	}
private:
	constexpr void deleteFix(Node* node) {
		while (node != root && node->color == Color::Black) {
			Node* parent = node->parent;
			if (node == parent->left) {
				Node* sibling = parent->right;
				if (sibling->color == Color::Red) {
					sibling->color = Color::Black;
					parent->color = Color::Red;
					rotateLeft(parent);
					sibling = parent->right;
				}
				if (sibling->left->color == Color::Black && sibling->right->color == Color::Black) {
					sibling->color = Color::Red;
					node = sibling->parent;
				}
				else {
					if (sibling->right->color == Color::Black) {
						sibling->left->color = Color::Black;
						sibling->color = Color::Red;
						rotateRight(sibling);
						sibling = parent->right;
					}
					sibling->color = parent->color;
					parent->color = Color::Black;
					sibling->right->color = Color::Black;
					rotateLeft(parent);
					node = root;
				}
			}
			else {
				Node* sibling = parent->left;
				if (sibling->color == Color::Red) {
					sibling->color = Color::Black;
					parent->color = Color::Red;
					rotateRight(parent);
					sibling = parent->left;
				}
				if (sibling->right->color == Color::Black && sibling->left->color == Color::Black) {
					sibling->color = Color::Red;
					node = sibling->parent;
				}
				else {
					if (sibling->left->color == Color::Black) {
						sibling->right->color = Color::Black;
						sibling->color = Color::Red;
						rotateLeft(sibling);
						sibling = parent->left;
					}
					sibling->color = parent->color;
					parent->color = Color::Black;
					sibling->left->color = Color::Black;
					rotateRight(parent);
					node = root;
				}
			}
		}
	}

	constexpr Node* min(Node* node) {
		while (node->left != &null) node = node->left;
		return node;
	}

	constexpr Node* max(Node* node) {
		while (node->right != &null) node = node->right;
		return node;
	}

	constexpr void insertFix(Node* node) {
		while (node->parent->color == Color::Red) {
			Node* parent = node->parent;
			Node* grand_parent = parent->parent;
			if (parent == grand_parent->left) {
				Node* uncle = parent->parent->right;
				if (uncle->color == Color::Red) {
					parent->color = Color::Black;
					uncle->color = Color::Black;
					grand_parent->color = Color::Red;
					node = grand_parent;
				}
				else {
					if (node == parent->right) {
						node = parent;
						rotateLeft(node);
					}
					node->parent->color = Color::Black;
					node->parent->parent->color = Color::Red;
					rotateRight(node->parent->parent);
				}
			}
			else {
				Node* uncle = parent->parent->left;
				if (uncle->color == Color::Red) {
					parent->color = Color::Black;
					uncle->color = Color::Black;
					grand_parent->color = Color::Red;
					node = grand_parent;
				}
				else {
					if (node == parent->left) {
						node = parent;
						rotateRight(node);
					}
					node->parent->color = Color::Black;
					node->parent->parent->color = Color::Red;
					rotateLeft(node->parent->parent);
				}
			}
		}

		root->color = Color::Black;
	}

	constexpr void rotateLeft(Node* node) {
		Node* new_root = node->right;
		node->right = new_root->left;
		if (new_root->left != &null) new_root->left->parent = node;
		new_root->parent = node->parent;
		if (node->parent == &null) root = new_root;
		else if (node == node->parent->left) node->parent->left = new_root;
		else node->parent->right = new_root;
		new_root->left = node;
		node->parent = new_root;
	}

	constexpr void rotateRight(Node* node) {
		Node* new_root = node->left;
		node->left = new_root->right;
		if (new_root->right != &null) new_root->right->parent = node;
		new_root->parent = node->parent;
		if (node->parent == &null) root = new_root;
		else if (node == node->parent->left) node->parent->left = new_root;
		else node->parent->right = new_root;
		new_root->right = node;
		node->parent = new_root;
	}

	constexpr void transplant(Node* replace, Node* with) {
		if (replace->parent == &null) root = with;
		else if (replace == replace->parent->left) replace->parent->left = with;
		else replace->parent->right = with;
		with->parent = replace->parent;
	}

	Node* root = &null;
	Node null {};
};