#pragma once
#include "vector.hpp"
#include "utils/spinlock.hpp"
#include "dev/event.hpp"

template<typename T>
struct RingBuffer {
	explicit RingBuffer(usize max_size) {
		buffer.get_unsafe().resize(max_size);
	}

	usize read(void* data, usize size) {
		usize orig_size = size;

		while (true) {
			usize to_read;

			IrqGuard irq_guard {};
			auto guard = buffer.lock();

			auto remaining_at_end = guard->size() - read_ptr;
			to_read = kstd::min(kstd::min(buffer_size, size), remaining_at_end);
			memcpy(data, guard->data() + read_ptr, to_read);
			read_ptr += to_read;
			buffer_size -= to_read;
			size -= to_read;
			if (read_ptr == guard->size()) {
				read_ptr = 0;
			}

			if (size && !buffer_size) {
				return orig_size - size;
			}

			if (!size) {
				break;
			}

			data = offset(data, void*, to_read);
		}

		return orig_size;
	}

	void read_block(void* data, usize size) {
		while (true) {
			usize to_read;
			bool wait = false;

			{
				IrqGuard irq_guard {};
				auto guard = buffer.lock();

				auto remaining_at_end = guard->size() - read_ptr;
				to_read = kstd::min(kstd::min(buffer_size, size), remaining_at_end);
				memcpy(data, guard->data() + read_ptr, to_read);
				read_ptr += to_read;
				buffer_size -= to_read;
				size -= to_read;
				if (read_ptr == guard->size()) {
					read_ptr = 0;
				}

				if (size && !buffer_size) {
					wait = true;
				}
			}

			if (wait) {
				write_event.signal_one();
				read_event.wait();
			}

			if (!size) {
				break;
			}

			data = offset(data, void*, to_read);
		}
	}

	usize write(const void* data, usize size) {
		usize orig_size = size;

		while (true) {
			usize to_write;

			IrqGuard irq_guard {};
			auto guard = buffer.lock();

			if (!buffer_size) {
				read_event.signal_one();
			}

			auto remaining_at_end = guard->size() - write_ptr;
			to_write = kstd::min(kstd::min(guard->size() - buffer_size, size), remaining_at_end);
			memcpy(guard->data() + write_ptr, data, to_write);
			write_ptr += to_write;
			buffer_size += to_write;
			size -= to_write;
			if (write_ptr == guard->size()) {
				write_ptr = 0;
			}

			if (size && buffer_size == guard->size()) {
				return orig_size - size;
			}

			if (!size) {
				break;
			}

			data = offset(data, void*, to_write);
		}

		return orig_size;
	}

	void write_block(const void* data, usize size) {
		while (true) {
			usize to_write;
			bool wait = false;

			{
				IrqGuard irq_guard {};
				auto guard = buffer.lock();

				if (!buffer_size) {
					read_event.signal_one();
				}

				auto remaining_at_end = guard->size() - write_ptr;
				to_write = kstd::min(kstd::min(guard->size() - buffer_size, size), remaining_at_end);
				memcpy(guard->data() + write_ptr, data, to_write);
				write_ptr += to_write;
				buffer_size += to_write;
				size -= to_write;
				if (write_ptr == guard->size()) {
					write_ptr = 0;
				}

				if (size && buffer_size == guard->size()) {
					wait = true;
				}
			}

			if (wait) {
				write_event.wait();
			}

			if (!size) {
				break;
			}

			data = offset(data, void*, to_write);
		}
	}

	usize size() {
		IrqGuard irq_guard {};
		auto guard = buffer.lock();
		return buffer_size;
	}

private:
	Spinlock<kstd::vector<T>> buffer;
	Event read_event {};
	Event write_event {};
	usize read_ptr {};
	usize write_ptr {};
	usize buffer_size {};
};
