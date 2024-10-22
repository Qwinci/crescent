#include "vfs.hpp"

kstd::shared_ptr<VNode> Vfs::lookup(kstd::string_view path) {
	if (path.is_empty()) {
		return nullptr;
	}

	kstd::shared_ptr<VNode> node = get_root();

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
