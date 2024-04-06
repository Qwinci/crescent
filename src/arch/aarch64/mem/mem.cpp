#include "aarch64_mem.hpp"
#include "mem/pmalloc.hpp"

void aarch64_mem_check_add_usable_range(usize base, usize len, MemReserve* reserve, u32 reserve_count) {
	auto end = base + len;

	for (u32 i = 0; i < reserve_count; ++i) {
		const auto& entry = reserve[i];
		auto entry_start = entry.base;
		auto entry_end = entry_start + entry.len;

		if (base < entry_end && end > entry_start) {
			if (entry_start <= base) {
				u64 used = entry_end - base;
				base = entry_end;
				len -= used;
			}
			else {
				u64 first_size = entry_start - base;
				u64 first_base = base;
				aarch64_mem_check_add_usable_range(first_base, first_size, reserve, reserve_count);

				if (entry_end < end) {
					u64 second_size = end - entry_end;
					u64 second_base = entry_end;
					aarch64_mem_check_add_usable_range(second_base, second_size, reserve, reserve_count);
				}
				return;
			}
		}
	}

	auto base_align = base & (0x1000 - 1);
	base = (base + 0x1000 - 1) & ~(0x1000 - 1);
	len -= base_align;
	len &= ~(0x1000 - 1);

	if (len < 0x1000) {
		return;
	}

	pmalloc_add_mem(base, len);
}
