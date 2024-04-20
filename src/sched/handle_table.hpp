#pragma once
#include "vector.hpp"
#include "variant.hpp"
#include "shared_ptr.hpp"
#include "unique_ptr.hpp"
#include "utils/spinlock.hpp"
#include "dev/dev.hpp"
#include "sys/socket.hpp"
#include "sched/ipc.hpp"
#include "sched/shared_mem.hpp"

struct ProcessDescriptor;
struct ThreadDescriptor;

using Handle = kstd::variant<
	kstd::monostate,
	kstd::shared_ptr<Device>,
	ProcessDescriptor,
	kstd::shared_ptr<ProcessDescriptor>,
	ThreadDescriptor,
	kstd::shared_ptr<Socket>,
	kstd::shared_ptr<SharedMemory>
	>;

class HandleTable {
public:
	kstd::shared_ptr<Handle> get(CrescentHandle handle);
	CrescentHandle insert(Handle&& handle);
	bool remove(CrescentHandle handle);

private:
	kstd::vector<kstd::shared_ptr<Handle>> table;
	CrescentHandle count {};
	kstd::vector<CrescentHandle> free_handles;
	Spinlock<void> lock {};
};
