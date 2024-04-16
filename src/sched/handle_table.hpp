#pragma once
#include "vector.hpp"
#include "variant.hpp"
#include "shared_ptr.hpp"
#include "unique_ptr.hpp"
#include "utils/spinlock.hpp"
#include "dev/dev.hpp"

struct Process;

using Handle = kstd::variant<
	kstd::monostate,
	kstd::shared_ptr<Device>,
	Process*
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
