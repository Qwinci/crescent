#pragma once

namespace kstd {
	template<typename T>
	class numeric_limits;
	template<>
	class numeric_limits<bool> {
	public:
		static constexpr int digits = 1;
	};
	template<>
	class numeric_limits<char> {
	public:
		static constexpr int digits = 7;
	};
	template<>
	class numeric_limits<unsigned char> {
	public:
		static constexpr int digits = 8;
	};
	template<>
	class numeric_limits<short> {
	public:
		static constexpr int digits = 8 * sizeof(short) - 1;
	};
	template<>
	class numeric_limits<unsigned short> {
	public:
		static constexpr int digits = 8 * sizeof(short);
	};
	template<>
	class numeric_limits<int> {
	public:
		static constexpr int digits = 8 * sizeof(int) - 1;
	};
	template<>
	class numeric_limits<unsigned int> {
	public:
		static constexpr int digits = 8 * sizeof(int);
	};
	template<>
	class numeric_limits<long> {
	public:
		static constexpr int digits = 8 * sizeof(long) - 1;
	};
	template<>
	class numeric_limits<unsigned long> {
	public:
		static constexpr int digits = 8 * sizeof(long);
	};
	template<>
	class numeric_limits<long long> {
	public:
		static constexpr int digits = 8 * sizeof(long long) - 1;
	};
	template<>
	class numeric_limits<unsigned long long> {
	public:
		static constexpr int digits = 8 * sizeof(long long);
	};
}
