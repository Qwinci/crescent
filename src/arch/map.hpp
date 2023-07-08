#pragma once
#include "types.hpp"

/// Converts phys to virtual using the hhdm
void* arch_to_virt(usize phys);
/// Converts virt to phys using the hhdm
usize arch_to_phys(void* virt);

namespace PageFlags {
	enum Type {
		Read,
		Write,
		Exec
	};
}

class PageMap {
public:
	static PageMap* create_user();
	virtual ~PageMap() = default;
	virtual bool map(void* virt, usize phys, usize size, PageFlags::Type flags) = 0;
	virtual bool unmap(void* virt, usize size) = 0;
	virtual void use() = 0;
};

extern PageMap* KERNEL_MAP;
