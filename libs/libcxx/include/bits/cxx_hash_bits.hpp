#ifndef _BITS_CXX_HASH_BITS_H
#define _BITS_CXX_HASH_BITS_H

#include <cstddef>
#include <cstdint>
#include <bit>

namespace std {
	namespace __detail {
		struct __fx_hasher64 {
			void __do_hash(uint64_t __word) {
				__hash = (rotl(__hash, 5) ^ __word) * 0x517CC1B727220A95;
			}

			void __do_hash(const void* __bytes, size_t __size) {
				auto* __ptr = bit_cast<const uint8_t*>(__bytes);

				while (__size >= 8) {
					uint64_t __value;
					__builtin_memcpy(&__value, __ptr, 8);
					__do_hash(__value);
					__ptr += 8;
					__size -= 8;
				}

				while (__size >= 4) {
					uint32_t __value;
					__builtin_memcpy(&__value, __ptr, 4);
					__do_hash(__value);
					__ptr += 4;
					__size -= 4;
				}

				while (__size >= 2) {
					uint16_t __value;
					__builtin_memcpy(&__value, __ptr, 2);
					__do_hash(__value);
					__ptr += 2;
					__size -= 2;
				}

				for (; __size; --__size, ++__ptr) {
					__do_hash(*__ptr);
				}
			}

			uint64_t __hash;
		};

		template<typename __T>
		inline size_t __int_hash(__T __value) {
			__detail::__fx_hasher64 __hasher {};
			__hasher.__do_hash(__value);
			return __hasher.__hash;
		}
	}

	template<typename __T>
	struct hash;

	template<>
	struct hash<signed char> {
		size_t operator()(signed char __value) const {
			return __detail::__int_hash(__value);
		}
	};

	template<>
	struct hash<unsigned char> {
		size_t operator()(unsigned char __value) const {
			return __detail::__int_hash(__value);
		}
	};

	template<>
	struct hash<short> {
		size_t operator()(short __value) const {
			return __detail::__int_hash(__value);
		}
	};

	template<>
	struct hash<unsigned short> {
		size_t operator()(unsigned short __value) const {
			return __detail::__int_hash(__value);
		}
	};

	template<>
	struct hash<int> {
		size_t operator()(int __value) const {
			return __detail::__int_hash(__value);
		}
	};

	template<>
	struct hash<unsigned int> {
		size_t operator()(unsigned int __value) const {
			return __detail::__int_hash(__value);
		}
	};

	template<>
	struct hash<long> {
		size_t operator()(long __value) const {
			return __detail::__int_hash(__value);
		}
	};

	template<>
	struct hash<unsigned long> {
		size_t operator()(unsigned long __value) const {
			return __detail::__int_hash(__value);
		}
	};

	template<>
	struct hash<long long> {
		size_t operator()(long long __value) const {
			return __detail::__int_hash(__value);
		}
	};

	template<>
	struct hash<unsigned long long> {
		size_t operator()(unsigned long long __value) const {
			return __detail::__int_hash(__value);
		}
	};

	template<>
	struct hash<bool> {
		size_t operator()(bool __value) const {
			return __detail::__int_hash(__value);
		}
	};

	template<>
	struct hash<char> {
		size_t operator()(char __value) const {
			return __detail::__int_hash(__value);
		}
	};

	template<>
	struct hash<char16_t> {
		size_t operator()(char16_t __value) const {
			return __detail::__int_hash(__value);
		}
	};

	template<>
	struct hash<char32_t> {
		size_t operator()(char32_t __value) const {
			return __detail::__int_hash(__value);
		}
	};
}

#endif
