#pragma once

#ifdef TESTING
#include <initializer_list>
#else
#include "initializer_list.hpp"
#endif

namespace kstd {
	template<typename T>
	constexpr const T& min(const T& a, const T& b) {
		return a < b ? a : b;
	}
	template<typename T, typename Compare>
	constexpr const T& min(const T& a, const T& b, Compare comp) {
		return comp(a, b) ? a : b;
	}
	template<typename T>
	constexpr T min(std::initializer_list<T> list) {
		const T* start = list.begin();
		const T* end = start + list.size();

		const T* smallest = start;
		for (; ++start != end;) {
			if (*start < *smallest) {
				smallest = start;
			}
		}
		return *smallest;
	}
	template<typename T, typename Compare>
	constexpr T min(std::initializer_list<T> list, Compare comp) {
		const T* start = list.begin();
		const T* end = start + list.size();

		const T* smallest = start;
		for (; ++start != end;) {
			if (comp(*start, *smallest)) {
				smallest = start;
			}
		}
		return *smallest;
	}

	template<typename T>
	constexpr const T& max(const T& a, const T& b) {
		return a > b ? a : b;
	}
	template<typename T, typename Compare>
	constexpr const T& max(const T& a, const T& b, Compare comp) {
		return comp(a, b) ? b : a;
	}
	template<typename T>
	constexpr T max(std::initializer_list<T> list) {
		const T* start = list.begin();
		const T* end = start + list.size();

		const T* largest = start;
		for (; ++start != end;) {
			if (*start > *largest) {
				largest = start;
			}
		}
		return *largest;
	}
	template<typename T, typename Compare>
	constexpr T max(std::initializer_list<T> list, Compare comp) {
		const T* start = list.begin();
		const T* end = start + list.size();

		const T* largest = start;
		for (; ++start != end;) {
			if (!comp(*start, *largest)) {
				largest = start;
			}
		}
		return *largest;
	}
}
