#include "vfs.hpp"

kstd::shared_ptr<VNode> vfs_lookup(kstd::shared_ptr<VNode> start, kstd::string_view path) {
	if (path.is_empty()) {
		return nullptr;
	}

	assert(start);
	auto node = std::move(start);

	size_t i = 0;
	while (path[i] == '/') {
		++i;
		if (i == path.size()) {
			return node;
		}
	}

	while (i < path.size()) {
		auto end = path.find('/', i);
		if (end == kstd::string_view::npos) {
			return node->lookup(path.substr(i, end));
		}
		else {
			auto slice = path.substr(i, (end - i));
			auto next = node->lookup(slice);
			if (!next) {
				return nullptr;
			}
			node = std::move(next);
			i = end + 1;
		}

		while (i < path.size() && path[i] == '/') {
			++i;
		}
	}

	return node;
}
