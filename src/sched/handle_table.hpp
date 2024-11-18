#pragma once
#include "dev/user_dev.hpp"
#include "fs/vfs.hpp"
#include "sched/ipc.hpp"
#include "sched/shared_mem.hpp"
#include "shared_ptr.hpp"
#include "sys/socket.hpp"
#include "unique_ptr.hpp"
#include "utils/spinlock.hpp"
#include "variant.hpp"
#include "vector.hpp"

struct ProcessDescriptor;
struct ThreadDescriptor;

struct EmptyHandle {};

struct OpenFile {
	constexpr explicit OpenFile(kstd::shared_ptr<VNode> node) : node {std::move(node)} {}

	kstd::shared_ptr<VNode> node;
	uint64_t cursor {};
};


using Handle = kstd::variant<
	kstd::monostate,
	EmptyHandle,
	DeviceHandle,
	kstd::shared_ptr<ProcessDescriptor>,
	kstd::shared_ptr<ThreadDescriptor>,
	kstd::shared_ptr<Socket>,
	kstd::shared_ptr<SharedMemory>,
	kstd::shared_ptr<OpenFile>
	>;

class HandleTable {
public:
	kstd::optional<Handle> get(CrescentHandle handle);
	CrescentHandle insert(Handle&& handle);
	bool replace(CrescentHandle replace, Handle&& with);
	bool remove(CrescentHandle handle);

private:
	kstd::vector<Handle> table;
	CrescentHandle count {};
	kstd::vector<CrescentHandle> free_handles;
	IrqSpinlock<void> lock {};
};
