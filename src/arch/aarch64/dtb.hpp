#pragma once
#include "types.hpp"
#include "bit.hpp"
#include "optional.hpp"
#include "string_view.hpp"
#include "span.hpp"

namespace dtb {
	struct FdtHeader {
		u32 magic;
		u32 total_size;
		u32 off_dt_struct;
		u32 off_dt_strings;
		u32 off_mem_rsvmap;
		u32 version;
		u32 last_comp_version;
		u32 boot_cpuid_phys;
		u32 size_dt_strings;
		u32 size_dt_struct;

		static FdtHeader parse(u32* ptr) {
			FdtHeader res {};
			res.magic = kstd::byteswap(ptr[0]);
			res.total_size = kstd::byteswap(ptr[1]);
			res.off_dt_struct = kstd::byteswap(ptr[2]);
			res.off_dt_strings = kstd::byteswap(ptr[3]);
			res.off_mem_rsvmap = kstd::byteswap(ptr[4]);
			res.version = kstd::byteswap(ptr[5]);
			res.last_comp_version = kstd::byteswap(ptr[6]);
			res.boot_cpuid_phys = kstd::byteswap(ptr[7]);
			res.size_dt_strings = kstd::byteswap(ptr[8]);
			res.size_dt_struct = kstd::byteswap(ptr[9]);
			return res;
		}
	};

	struct FdtReserveEntry {
		u64 addr;
		u64 size;
	};

	struct Prop {
		void* ptr;
		u32 len;

		template<kstd::integral T>
		[[nodiscard]] inline T read(u32 offset) const {
			assert(offset + sizeof(T) <= len);

			if constexpr (sizeof(T) == 8) {
				u32 first = *offset(ptr, const u32*, offset);
				u64 second = *offset(ptr, const u32*, offset + 4);
				u64 final = first | second << 32;
				return kstd::byteswap(final);
			}
			else {
				return kstd::byteswap(*offset(ptr, const T*, offset));
			}
		}

		[[nodiscard]] inline u64 read_cells(u32 offset, u32 cells) const {
			assert(offset + cells * 4 <= len);
			if (cells == 1) {
				return read<u32>(offset);
			}
			else if (cells == 2) {
				return read<u64>(offset);
			}
			else {
				__builtin_trap();
			}
		}
	};

	struct SizeAddrCells {
		u32 size_cells;
		u32 addr_cells;
	};

	class Dtb;

	struct Node {
		Node(const Dtb* dtb, u32* start);

		[[nodiscard]] kstd::optional<Prop> prop(kstd::string_view prop_name) const;
		[[nodiscard]] kstd::optional<Node> next() const;
		[[nodiscard]] kstd::optional<Node> child() const;
		[[nodiscard]] kstd::optional<Node> find_child(kstd::string_view child_name) const;
		[[nodiscard]] kstd::optional<kstd::string_view> compatible() const;

		struct Reg {
			u64 addr;
			u64 size;
		};
		[[nodiscard]] kstd::optional<Reg> reg(const SizeAddrCells& cells, u32 index) const;

		[[nodiscard]] SizeAddrCells size_cells() const;

		kstd::string_view name {};
		kstd::string_view unit {};

		const Dtb* dtb;

		template<typename Node>
		struct ChildIterator {
			struct Iterator {
				constexpr bool operator!=(const Iterator& other) const {
					return node.has_value() != other.node.has_value();
				}

				constexpr Iterator& operator++() {
					node = node.value().next();
					return *this;
				}

				constexpr Node& operator*() {
					return node.value();
				}

				kstd::optional<Node> node;
			};

			Iterator begin() {
				return Iterator {node.child()};
			}

			Iterator end() {
				return Iterator {};
			}

			Node node;
		};

		ChildIterator<Node> child_iter();

		u32* ptr;
	};

	class Dtb {
	public:
		class ReservedEntryIterator {
		public:
			class Iterator {
			public:
				constexpr Iterator& operator++() {
					++ptr;
					return *this;
				}

				constexpr const FdtReserveEntry& operator*() const {
					return *ptr;
				}

				constexpr bool operator!=(const Iterator&) const {
					if (!ptr->addr && !ptr->size) {
						return false;
					}
					return true;
				}

			private:
				friend ReservedEntryIterator;
				constexpr explicit Iterator(const FdtReserveEntry* ptr) : ptr {ptr} {}

				const FdtReserveEntry* ptr;
			};

			[[nodiscard]] constexpr Iterator begin() const {
				return Iterator {ptr};
			}

			[[nodiscard]] constexpr Iterator end() const { // NOLINT(*-convert-member-functions-to-static)
				return Iterator {nullptr};
			}

		private:
			friend class Dtb;
			constexpr explicit ReservedEntryIterator(const FdtReserveEntry* ptr) : ptr {ptr} {}

			const FdtReserveEntry* ptr;
		};

		explicit Dtb(void* ptr);
		Node get_root();

		ReservedEntryIterator get_reserved();

		SizeAddrCells size_cells {.size_cells = 1, .addr_cells = 2};
		size_t total_size;
	private:
		u32* root_ptr;
		FdtReserveEntry* reserved;
		friend struct Node;

	public:
		const char* strings;

		static u32* next(u32* ptr);
		static u32* next_child(u32* ptr);
		static u32* next_prop(u32* ptr);
	};
}
