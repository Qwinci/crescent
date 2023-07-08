#pragma once
#include "arch/map.hpp"

class X86PageMap : public PageMap {
public:
	bool map(void* virt, usize phys, usize size, PageFlags::Type flags) override;
	bool unmap(void* virt, usize size) override;
	void use() override;
private:
	struct Page* pml4_page;
	friend PageMap* PageMap::create_user();
};
