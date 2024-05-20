#include "handle_table.hpp"
#include "process.hpp"

kstd::optional<Handle> HandleTable::get(CrescentHandle handle) {
	auto guard = lock.lock();

	if (handle >= count) {
		return {};
	}

	auto loc = table[handle];
	if (loc.get<kstd::monostate>()) {
		return {};
	}
	return std::move(loc);
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

	table[index] = std::move(handle);

	return index;
}

bool HandleTable::replace(CrescentHandle replace, Handle&& with) {
	auto guard = lock.lock();

	if (replace >= count) {
		return false;
	}

	auto& loc = table[replace];
	if (loc.get<kstd::monostate>()) {
		return false;
	}
	loc = std::move(with);
	return true;
}

bool HandleTable::remove(CrescentHandle handle) {
	auto guard = lock.lock();

	if (handle >= count) {
		return false;
	}

	auto& loc = table[handle];
	if (loc.get<kstd::monostate>()) {
		return false;
	}
	loc = kstd::monostate {};
	free_handles.push(handle);

	return true;
}
