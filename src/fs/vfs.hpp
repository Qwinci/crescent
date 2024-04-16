#pragma once
#include "string_view.hpp"
#include "shared_ptr.hpp"

enum class FsStatus {
	Success,
	Unsupported,
	OutOfBounds
};

struct FsStat {
	usize size;
};

struct VNode {
	virtual ~VNode() = default;

	virtual kstd::shared_ptr<VNode> lookup(kstd::string_view name) = 0;
	virtual FsStatus stat(FsStat& data) = 0;
	virtual FsStatus read(void* data, usize size, usize offset) = 0;
};

struct Vfs {
	virtual kstd::shared_ptr<VNode> get_root() = 0;

	kstd::shared_ptr<VNode> lookup(kstd::string_view path);
};

extern Vfs* INITRD_VFS;
