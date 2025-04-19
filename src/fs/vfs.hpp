#pragma once
#include "string_view.hpp"
#include "shared_ptr.hpp"
#include "utils/flags_enum.hpp"
#include "dev/event.hpp"

enum class FsStatus {
	Success,
	Unsupported,
	OutOfBounds,
	TryAgain
};

enum class FileFlags {
	None = 0,
	NonBlock = 1 << 0
};

FLAGS_ENUM(FileFlags);

struct FsStat {
	usize size;
};

struct DirEntry {
	char name[128];
	usize name_len;

	enum class Type {
		File,
		Directory
	};
	Type type;
};

enum class PollEvent {
	None,
	In = 1 << 0,
	Out = 1 << 1,
	Hup = 1 << 2,
	Err = 1 << 3,
	UrgentOut = 1 << 4
};
FLAGS_ENUM(PollEvent);

struct VNode {
	constexpr explicit VNode(FileFlags flags, bool seekable)
		: seekable {seekable}, flags {flags} {}

	virtual ~VNode() = default;

	virtual kstd::shared_ptr<VNode> lookup(kstd::string_view) {
		return nullptr;
	}

	virtual FsStatus stat(FsStat& data) {
		return FsStatus::Unsupported;
	}

	virtual FsStatus list_dir(DirEntry* entries, usize& count, usize& offset) {
		return FsStatus::Unsupported;
	}

	virtual FsStatus read(void* data, usize& size, usize offset) {
		return FsStatus::Unsupported;
	}

	virtual FsStatus write(const void* data, usize& size, usize offset) {
		return FsStatus::Unsupported;
	}

	virtual FsStatus poll(PollEvent& events) {
		return FsStatus::Unsupported;
	}

	Event poll_event {};
	bool seekable {};

protected:
	FileFlags flags;
};

struct Vfs {
	virtual kstd::shared_ptr<VNode> get_root() = 0;
};

kstd::shared_ptr<VNode> vfs_lookup(kstd::shared_ptr<VNode> start, kstd::string_view path);

extern Vfs* INITRD_VFS;
