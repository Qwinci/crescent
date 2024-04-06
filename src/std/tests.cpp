#include "vector.hpp"
#include "vmem.hpp"
#include <gtest/gtest.h>
#include <utility>
#include <vector>

TEST(vmem_suite, vmem_tests) {
	VMem vmem {};
	vmem.init(0xFFFF000000000000, 0xFFFFFFFF80000000 - 0xFFFF000000000000, 0x1000);

	std::vector<std::pair<usize, usize>> allocations;
	int count = rand() % 5000;
	for (int i = 0; i < count; ++i) {
		int size = rand() % (1024 * 1024);
		auto mem = vmem.xalloc(size, 0, 0);
		EXPECT_NE(mem, 0);
		allocations.emplace_back(mem, size);
	}

	for (int i = 0; i < count - 2; i += 2) {
		auto alloc = allocations[i];
		vmem.xfree(alloc.first, alloc.second);
		allocations[i] = {0, 0};
	}

	for (int i = 0; i < count; ++i) {
		auto alloc = allocations[i];
		if (alloc.first) {
			vmem.xfree(alloc.first, alloc.second);
		}
	}

	vmem.destroy(true);

	vmem.init(0x1000, 0x1000, 0x1000);
	auto ptr0 = vmem.xalloc(0x1000, 0, 0);
	EXPECT_EQ(ptr0, 0x1000);
	EXPECT_EQ(vmem.xalloc(0x1000, 0, 0), 0);
	vmem.xfree(ptr0, 0x1000);
	ptr0 = vmem.xalloc(0x1000, 0, 0);
	EXPECT_EQ(ptr0, 0x1000);
	vmem.xfree(ptr0, 0x1000);

	vmem.destroy(true);
}

TEST(memory, vector) {
	kstd::vector<int> vec {};
	auto count = rand() % 2000;
	for (int i = 0; i < count; ++i) {
		vec.push(i);
	}

	vec.remove(0);
	vec.remove(vec.size() - 1);

	kstd::vector<int> other {};
	other.push(10);
	vec = other;
}
