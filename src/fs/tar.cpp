#include "tar.hpp"
#include "assert.hpp"
#include "mem/mem.hpp"
#include "vfs.hpp"
#include "cstring.hpp"

struct TarHeader {
	char name[100];
	char mode[8];
	char uid[8];
	char gid[8];
	char size[12];
	char mtime[12];
	char chk_sum[8];
	char type_flag;
	char linkname[100];
	char magic[6];
	char version[2];
	char uname[32];
	char gname[32];
	char devmajor[8];
	char devminor[8];
	char prefix[155];
};

static u64 parse_oct(const char* ptr) {
	kstd::string_view str {ptr};
	u64 value = 0;
	for (auto c : str) {
		value = value * 8 + (c - '0');
	}
	return value;
}

struct TarFileNode : public VNode {
	constexpr explicit TarFileNode(const TarHeader* hdr)
		: VNode {FileFlags::None, true}, hdr {hdr} {}

	kstd::shared_ptr<VNode> lookup(kstd::string_view name) override {
		if (hdr->type_flag != '5') {
			return nullptr;
		}

		kstd::string_view dir_name {hdr->name};

		usize offset = 512 + ALIGNUP(parse_oct(hdr->size), 512);
		while (true) {
			auto* next = offset(hdr, const TarHeader*, offset);
			if (!*next->name) {
				return nullptr;
			}

			kstd::string_view next_name {next->name};
			if (!next_name.remove_prefix(dir_name)) {
				return nullptr;
			}
			next_name.remove_suffix("/");

			if (next_name == name) {
				return kstd::make_shared<TarFileNode>(next);
			}

			offset += 512 + ALIGNUP(parse_oct(next->size), 512);
		}
	}

	FsStatus read(void* data, usize& size, usize offset) override {
		if (hdr->type_flag != '0') {
			return FsStatus::Unsupported;
		}

		auto max_size = parse_oct(hdr->size);
		if (offset + size > max_size) {
			return FsStatus::OutOfBounds;
		}

		memcpy(data, offset(hdr, const void*, 512 + offset), size);

		return FsStatus::Success;
	}

	FsStatus stat(FsStat& data) override {
		if (hdr->type_flag != '0') {
			return FsStatus::Unsupported;
		}

		data.size = parse_oct(hdr->size);

		return FsStatus::Success;
	}

	FsStatus list_dir(DirEntry* entries, usize& count, usize& offset) override {
		if (hdr->type_flag != '5') {
			return FsStatus::Unsupported;
		}

		kstd::string_view dir_name {hdr->name};

		usize actual_offset = 512 + ALIGNUP(parse_oct(hdr->size), 512);
		actual_offset += offset;
		usize actual_count = 0;

		while (true) {
			auto* next = offset(hdr, const TarHeader*, actual_offset);
			if (!*next->name) {
				break;
			}

			kstd::string_view next_name {next->name};
			auto orig_next_name = next_name;
			if (!next_name.remove_prefix(dir_name)) {
				break;
			}
			next_name.remove_suffix("/");

			if (actual_count < count) {
				orig_next_name.remove_prefix(".");
				orig_next_name.remove_suffix("/");

				auto& entry = entries[actual_count++];
				auto to_copy = kstd::min(orig_next_name.size(), sizeof(entry.name) - 1);
				memcpy(entry.name, orig_next_name.data(), to_copy);
				entry.name[to_copy] = 0;
				entry.name_len = orig_next_name.size();
				entry.type = next->type_flag == '0' ? DirEntry::Type::File : DirEntry::Type::Directory;

				if (actual_count == count) {
					break;
				}
			}
			else {
				++actual_count;
			}

			actual_offset += 512 + ALIGNUP(parse_oct(next->size), 512);
		}

		count = actual_count;
		offset = actual_offset;

		return FsStatus::Success;
	}

	const TarHeader* hdr;
};

struct TarVfs : public Vfs {
	kstd::shared_ptr<VNode> get_root() override {
		return kstd::make_shared<TarFileNode>(hdr);
	}

	const TarHeader* hdr {};
};

static TarVfs TAR_INITRD_VFS {};
Vfs* INITRD_VFS = &TAR_INITRD_VFS;

void tar_initramfs_init(const void* data, usize) {
	TAR_INITRD_VFS.hdr = static_cast<const TarHeader*>(data);
	assert(kstd::string_view {TAR_INITRD_VFS.hdr->magic, 5} == "ustar");
}
