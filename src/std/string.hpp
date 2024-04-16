#pragma once
#include "vector.hpp"
#include "string_view.hpp"

namespace kstd {
	template<typename T>
	class basic_string : public vector<T> {
	public:
		static constexpr size_t npos = static_cast<size_t>(-1);

		constexpr basic_string() = default;
		constexpr basic_string(basic_string_view<T> str) { // NOLINT(*-explicit-constructor)
			vector<T>::resize(str.size() + 1);
			for (usize i = 0; i < str.size(); ++i) {
				vector<T>::operator[](i) = str[i];
			}
			vector<T>::operator[](str.size()) = 0;
		}

		constexpr basic_string& operator=(basic_string_view<T> str) {
			vector<T>::clear();
			vector<T>::resize(str.size() + 1);
			for (usize i = 0; i < str.size(); ++i) {
				vector<T>::operator[](i) = str[i];
			}
			vector<T>::operator[](str.size()) = 0;
			return *this;
		}

		constexpr basic_string& operator+(basic_string_view<T> str) {
			auto start = vector<T>::size() - 1;
			vector<T>::resize(start + str.size() + 1);
			for (size_t i = 0; i < str.size(); ++i) {
				vector<T>::operator[](start + i) = str[i];
			}
			vector<T>::operator[](start + str.size()) = 0;
			return *this;
		}

		constexpr basic_string& operator+=(basic_string_view<T> str) {
			return *this + str;
		}

		constexpr operator basic_string_view<T>() const { // NOLINT(*-explicit-constructor)
			return as_view();
		};

		constexpr basic_string_view<T> as_view() const {
			return basic_string_view<T> {this->data(), this->size() - 1};
		}

		void resize_without_null(size_t new_size) {
			vector<T>::resize(new_size + 1);
		}

		constexpr bool operator==(const basic_string& other) const {
			return as_view() == other.as_view();
		}

		constexpr bool operator==(const basic_string_view<T>& other) const {
			return as_view() == other;
		}

		static constexpr optional<basic_string> from(kstd::basic_string_view<T> str) {
			basic_string res {};
			res.resize(str.size() + 1);
			for (usize i = 0; i < str.size(); ++i) {
				res[i] = str[i];
			}
			res[str.size()] = 0;
			return res;
		}

		explicit basic_string(kstd::wstring_view str) requires(is_same_v<T, char>) {
			for (usize i = 0; i < str.size(); ++i) {
				u32 cp;
				if (str[i] <= 0xD7FF || (str[i] >= 0xE000 && str[i] <= 0xFFFF)) {
					cp = str[i];
				}
				else {
					u32 high = static_cast<u32>(str[i] - 0xD800) * 0x400;
					u32 low = static_cast<u32>(str[i + 1] - 0xDC00);
					cp = high + low + 0x10000;
					++i;
				}

				if (cp <= 0x7F) {
					vector<T>::push(cp);
				}
				else if (cp <= 0x7FF) {
					vector<T>::reserve(2);
					vector<T>::push((0b110 << 5) | (cp >> 6 & 0b11111));
					vector<T>::push((0b10 << 6) | (cp & 0b111111));
				}
				else if (cp <= 0xFFFF) {
					vector<T>::reserve(3);
					vector<T>::push((0b1110 << 4) | (cp >> 12 & 0b1111));
					vector<T>::push((0b10 << 6) | (cp >> 6 & 0b111111));
					vector<T>::push((0b10 << 6) | (cp & 0b111111));
				}
				else if (cp <= 0x10FFFF) {
					vector<T>::reserve(4);
					vector<T>::push((0b11110 << 3) | (cp >> 18 & 0b111));
					vector<T>::push((0b10 << 6) | (cp >> 12 & 0b111111));
					vector<T>::push((0b10 << 6) | (cp >> 6 & 0b111111));
					vector<T>::push((0b10 << 6) | (cp & 0b111111));
				}
			}

			vector<T>::push(0);
		}
	};

	using string = basic_string<char>;
	using wstring = basic_string<wchar_t>;
}
