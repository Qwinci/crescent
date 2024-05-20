#pragma once
#include "cstdint.hpp"
#include "vector.hpp"
#include "optional.hpp"

namespace kstd {
	template<typename T, typename K>
	class unordered_map {
	public:
		void remove(K key) {
			auto hash = fnv_hash(&key, sizeof(K));

			size_t table_size = table.size();
			size_t bucket_index = hash % table_size;
			size_t start = bucket_index;
			while (true) {
				auto& bucket = table[bucket_index];
				if (bucket && bucket.value().key == key) {
					bucket.reset();
					return;
				}
				bucket_index = (bucket_index + 1) % table_size;
				if (bucket_index == start) {
					break;
				}
			}
		}

		void insert(K key, const T& value) {
			auto hash = fnv_hash(&key, sizeof(K));
			if (table.is_empty()) {
				table.resize(8);
				table[hash % 8] = Element {.key {key}, .value {value}};
				++used;
				return;
			}

			auto load_factor = used * 100 / table.size();
			if (load_factor >= 70) {
				kstd::vector<kstd::optional<Element>> new_table;
				size_t new_table_size = table.size() * 2;
				new_table.resize(new_table_size);

				for (auto& elem : table) {
					if (!elem.has_value()) {
						continue;
					}
					auto elem_hash = fnv_hash(&elem.value().key, sizeof(K));
					size_t bucket_index = elem_hash % new_table_size;
					while (true) {
						auto& bucket = new_table[bucket_index];
						if (!bucket.has_value()) {
							bucket = std::move(elem);
							break;
						}
						bucket_index = (bucket_index + 1) % new_table_size;
					}
				}

				table = std::move(new_table);
			}

			size_t table_size = table.size();
			size_t bucket_index = hash % table_size;
			while (true) {
				auto& bucket = table[bucket_index];
				if (!bucket.has_value() || bucket->key == key) {
					bucket = Element {.key {key}, .value {value}};
					++used;
					break;
				}
				bucket_index = (bucket_index + 1) % table_size;
			}
		}

		void insert(K key, T&& value) {
			auto hash = fnv_hash(&key, sizeof(K));
			if (table.is_empty()) {
				table.resize(8);
				table[hash % 8] = Element {.key {key}, .value {move(value)}};
				return;
			}

			auto load_factor = used * 100 / table.size();
			if (load_factor >= 70) {
				kstd::vector<kstd::optional<Element>> new_table;
				size_t new_table_size = table.size() * 2;
				new_table.reserve(table.size() * 2);

				for (auto& elem : table) {
					if (!elem.has_value()) {
						continue;
					}
					auto elem_hash = fnv_hash(&elem.value().key, sizeof(K));
					size_t bucket_index = elem_hash % new_table_size;
					while (true) {
						auto& bucket = new_table[bucket_index];
						if (!bucket.has_value()) {
							bucket = move(elem);
							break;
						}
						bucket_index = (bucket_index + 1) % new_table_size;
					}
				}

				table = move(new_table);
			}

			size_t table_size = table.size();
			size_t bucket_index = hash % table_size;
			while (true) {
				auto& bucket = table[bucket_index];
				if (!bucket.has_value() || bucket->key == key) {
					bucket = Element {.key {key}, .value {move(value)}};
					break;
				}
				bucket_index = (bucket_index + 1) % table_size;
			}
		}

		[[nodiscard]] T* get(K key) {
			auto hash = fnv_hash(&key, sizeof(K));

			size_t table_size = table.size();
			size_t bucket_index = hash % table_size;
			size_t start = bucket_index;
			while (true) {
				auto& bucket = table[bucket_index];
				if (bucket && bucket.value().key == key) {
					return &bucket->value;
				}
				bucket_index = (bucket_index + 1) % table_size;
				if (bucket_index == start) {
					break;
				}
			}

			return nullptr;
		}

		[[nodiscard]] const T* get(K key) const {
			auto hash = fnv_hash(&key, sizeof(K));

			size_t table_size = table.size();
			size_t bucket_index = hash % table_size;
			size_t start = bucket_index;
			while (true) {
				auto& bucket = table[bucket_index];
				if (bucket && bucket.value().key == key) {
					return &bucket->value;
				}
				bucket_index = (bucket_index + 1) % table_size;
				if (bucket_index == start) {
					break;
				}
			}

			return nullptr;
		}

	private:
		static uint64_t fnv_hash(const void* data, size_t size) {
			uint64_t hash = 0xCBF29CE484222000;
			auto* ptr = static_cast<const uint8_t*>(data);
			for (size_t i = 0; i < size; ++i) {
				hash ^= ptr[i];
				hash *= 0x100000001B3;
			}
			return hash;
		}

		struct Element {
			K key;
			T value;
		};

		kstd::vector<kstd::optional<Element>> table;
		size_t used;
	};
}
