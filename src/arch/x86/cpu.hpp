#pragma once
#include "types.hpp"

struct Msr {
	constexpr explicit Msr(u32 offset) : offset {offset} {}

	[[nodiscard]] inline u64 read() const {
		u32 low;
		u32 high;
		asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(offset));
		return static_cast<u64>(low) | static_cast<u64>(high) << 32;
	}

	inline void write(u64 value) const {
		u32 low = value;
		u32 high = value >> 32;
		asm volatile("wrmsr" : : "a"(low), "d"(high), "c"(offset));
	}

	u32 offset;
};

namespace msrs {
	constexpr Msr IA32_APIC_BASE {0x1B};
	constexpr Msr IA32_MTRRCAP {0xFE};
	constexpr Msr IA32_PAT {0x277};
	constexpr Msr IA32_MTRRDEFTYPE {0x2FF};
	constexpr Msr IA32_EFER {0xC0000080};
	constexpr Msr IA32_STAR {0xC0000081};
	constexpr Msr IA32_LSTAR {0xC0000082};
	constexpr Msr IA32_CSTAR {0xC0000083};
	constexpr Msr IA32_SFMASK {0xC0000084};
	constexpr Msr IA32_FSBASE {0xC0000100};
	constexpr Msr IA32_GSBASE {0xC0000101};
	constexpr Msr IA32_KERNEL_GSBASE {0xC0000102};
}

inline void xsave(u8* area, u64 mask) {
	assert(reinterpret_cast<usize>(area) % 64 == 0);
	u32 low = mask & 0xFFFFFFFF;
	u32 high = mask >> 32;
	asm volatile("xsaveq %0" : : "m"(*area), "a"(low), "d"(high) : "memory");
}

inline void xrstor(u8* area, u64 mask) {
	assert(reinterpret_cast<usize>(area) % 64 == 0);
	u32 low = mask & 0xFFFFFFFF;
	u32 high = mask >> 32;
	asm volatile("xrstorq %0" : : "m"(*area), "a"(low), "d"(high) : "memory");
}

inline void wrxcr(u32 index, u64 value) {
	u32 low = value;
	u32 high = value >> 32;
	asm volatile("xsetbv" : : "c"(index), "a"(low), "d"(high) : "memory");
}

struct CpuFeatures {
	usize xsave_area_size;
	bool rdrnd;
	bool xsave;
	bool avx;
	bool avx512;
	bool umip;
	bool smep;
	bool smap;
	bool rdseed;
};
static_assert(offsetof(CpuFeatures, smap) == 14);

[[gnu::visibility("hidden")]] extern CpuFeatures CPU_FEATURES;
