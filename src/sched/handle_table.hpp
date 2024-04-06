#pragma once
#include "vector.hpp"
#include "variant.hpp"
#include "shared_ptr.hpp"
#include "utils/spinlock.hpp"
#include "dev/dev.hpp"

using Handle = kstd::variant<
	kstd::monostate,
	kstd::shared_ptr<Device>
	>;

class HandleTable {
public:
	kstd::shared_ptr<Handle> get(usize handle);
	usize insert(Handle&& handle);
	bool remove(usize handle);

private:
	kstd::vector<kstd::shared_ptr<Handle>> table;
	usize count {};
	kstd::vector<usize> free_handles;
	Spinlock<void> lock {};
};
