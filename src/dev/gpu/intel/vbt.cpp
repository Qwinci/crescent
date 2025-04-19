#include "vbt.hpp"
#include "cstring.hpp"

namespace vbt {
	u32 get_block_size(const void* base) {
		auto* ptr = static_cast<const u8*>(base);

		if (static_cast<BdbBlockId>(*ptr) == BdbBlockId::MipiSequence &&
			ptr[3] >= 3) {
			u32 size;
			memcpy(&size, ptr + 4, 4);
			return size;
		}
		else {
			u16 size;
			memcpy(&size, ptr + 1, 2);
			return size;
		}
	}

	const void* get_block(const BdbHeader* hdr, BdbBlockId id) {
		u32 offset = hdr->header_size;
		u32 max_size = hdr->bdb_size;

		while (offset + 3 < max_size) {
			auto* ptr = offset(hdr, const u8*, offset);

			u8 block_id = *ptr;
			u32 size = get_block_size(ptr);

			offset += 3;

			if (offset + size > max_size) {
				return nullptr;
			}

			if (block_id == static_cast<u8>(id)) {
				return ptr;
			}

			offset += size;
		}

		return nullptr;
	}
}
