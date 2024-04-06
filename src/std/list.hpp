#pragma once

struct ListHook {
	void* next;
};

template<typename T, ListHook (T::*Hook)>
class List {
public:
	class Iterator {
	private:
		constexpr explicit Iterator(T* ptr) : ptr {ptr} {}

		T* ptr;
		friend class List;
	public:
		constexpr bool operator!=(const Iterator& other) const {
			return ptr != other.ptr;
		}

		constexpr Iterator& operator++() {
			ptr = static_cast<T*>((ptr->*Hook).next);
			return *this;
		}

		constexpr T& operator*() {
			return *ptr;
		}
	};

	class ConstIterator {
	private:
		constexpr explicit ConstIterator(const T* ptr) : ptr {ptr} {}

		const T* ptr;
		friend class List;
	public:
		constexpr bool operator!=(const Iterator& other) const {
			return ptr != other.ptr;
		}

		constexpr Iterator& operator++() {
			ptr = static_cast<const T*>((ptr->*Hook).next);
			return *this;
		}

		constexpr const T& operator*() const {
			return *ptr;
		}
	};

	constexpr void push_front(T* value) {
		(value->*Hook).next = root;
		if (!root) {
			_end = value;
		}
		root = value;
	}

	constexpr T* pop_front() {
		if (!root) {
			return nullptr;
		}
		auto node = root;
		root = static_cast<T*>((node->*Hook).next);
		if (!root) {
			_end = nullptr;
		}
		return node;
	}

	constexpr void push(T* value) {
		(value->*Hook).next = nullptr;
		if (_end) {
			(_end->*Hook).next = value;
		}
		else {
			root = value;
		}
		_end = value;
	}

	template<typename Compare>
	[[nodiscard]] constexpr T* find(Compare comp) {
		for (auto iter = begin(); iter != end(); ++iter) {
			if (comp(*iter)) {
				return &*iter;
			}
		}
		return nullptr;
	}

	[[nodiscard]] constexpr bool is_empty() const {
		return root;
	}

	[[nodiscard]] constexpr Iterator begin() {
		return Iterator {root};
	}

	[[nodiscard]] constexpr Iterator end() {
		return Iterator {_end};
	}

	[[nodiscard]] constexpr ConstIterator begin() const {
		return ConstIterator {root};
	}

	[[nodiscard]] constexpr ConstIterator end() const {
		return ConstIterator {_end};
	}

private:
	T* root;
	T* _end;
};
