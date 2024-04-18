#include "handle_table.hpp"
#include "process.hpp"

kstd::shared_ptr<Handle> HandleTable::get(CrescentHandle handle) {
	auto guard = lock.lock();

	if (handle >= count) {
		return nullptr;
	}

	auto& loc = table[handle];
	if (loc->get<kstd::monostate>()) {
		return nullptr;
	}
	return loc;
}

CrescentHandle HandleTable::insert(Handle&& handle) {
	auto guard = lock.lock();

	CrescentHandle index;
	if (!free_handles.is_empty()) {
		index = free_handles.pop().value();
	}
	else {
		if (count == table.size()) {
			auto new_size = table.size() < 8 ? 8 : (table.size() + table.size() / 2);
			table.resize(new_size);
		}
		index = count++;
	}

	table[index] = kstd::shared_ptr<Handle> {std::move(handle)};

	return index;
}

bool HandleTable::remove(CrescentHandle handle) {
	auto guard = lock.lock();

	if (handle >= count) {
		return false;
	}

	auto& loc = table[handle];
	if (loc->get<kstd::monostate>()) {
		return false;
	}
	loc = kstd::shared_ptr<Handle> {Handle {kstd::monostate {}}};
	free_handles.push(handle);

	return true;
}
