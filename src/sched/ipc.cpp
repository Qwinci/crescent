#include "ipc.hpp"
#include "sched/process.hpp"

IpcSocket::IpcSocket(kstd::shared_ptr<ProcessDescriptor> owner_desc, int flags)
	: Socket {flags}, owner_desc {std::move(owner_desc)} {}

IpcSocket::~IpcSocket() {
	disconnect();
}

int IpcSocket::connect(const AnySocketAddress& address) {
	disconnect();

	if (address.generic.type != SOCKET_ADDRESS_TYPE_IPC) {
		return ERR_INVALID_ARGUMENT;
	}

	{
		auto descriptor = address.ipc.descriptor;
		IrqGuard irq_guard {};
		auto process_guard = descriptor->process.lock();
		if (!process_guard) {
			return ERR_INVALID_ARGUMENT;
		}

		target_address.generic.type = SOCKET_ADDRESS_TYPE_IPC;
		target_address.descriptor = new ProcessDescriptor {*process_guard, 0};

		(*process_guard)->add_descriptor(target_address.descriptor);

		auto target_socket_guard = (*process_guard)->ipc_socket.lock();
		if (!*target_socket_guard) {
			auto desc = kstd::make_shared<ProcessDescriptor>(*process_guard, 0);
			(*process_guard)->add_descriptor(desc.data());

			*target_socket_guard = kstd::make_shared<IpcSocket>(std::move(desc), flags);
		}
		auto target_guard = (*target_socket_guard)->lock.lock();
		(*target_socket_guard)->pending = this;
		(*target_socket_guard)->pending_event.signal_one();
	}

	pending_event.wait();
	return 0;
}

int IpcSocket::disconnect() {
	IrqGuard irq_guard {};
	auto guard = lock.lock();

	if (!target) {
		return 0;
	}

	auto target_lock = target->lock.lock();
	assert(target->target == this);
	target->target = nullptr;
	target->target_address.descriptor = nullptr;
	target = nullptr;

	delete target_address.descriptor;
	target_address.descriptor = nullptr;

	return 0;
}

int IpcSocket::listen(uint32_t) {
	if (flags & SOCK_NONBLOCK) {
		IrqGuard irq_guard {};
		auto guard = lock.lock();
		if (!pending) {
			return ERR_TRY_AGAIN;
		}
		return 0;
	}

	pending_event.wait();
	IrqGuard irq_guard {};
	auto guard = lock.lock();
	assert(pending);
	return 0;
}

int IpcSocket::accept(kstd::shared_ptr<Socket>& connection, int connection_flags) {
	IrqGuard irq_guard {};
	auto guard = lock.lock();
	if (!pending) {
		return ERR_INVALID_ARGUMENT;
	}

	auto ipc_socket = kstd::make_shared<IpcSocket>(owner_desc, connection_flags);
	auto target_guard = pending->lock.lock();

	ipc_socket->target = pending;
	ipc_socket->target_address = {
		.generic {
			.type = SOCKET_ADDRESS_TYPE_IPC
		},
		.descriptor = pending->owner_desc.data()
	};
	pending->target = ipc_socket.data();
	pending->pending_event.signal_one();
	pending = nullptr;
	connection = std::move(ipc_socket);
	return 0;
}

int IpcSocket::send(const void* data, usize& size) {
	IrqGuard irq_guard {};
	auto guard = lock.lock();

	if (!target) {
		return ERR_INVALID_ARGUMENT;
	}

	usize orig_size = size;

	while (true) {
		{
			auto target_guard = target->lock.lock();

			if (!target->buf_size) {
				target->write_event.signal_one();
			}

			usize to_write = kstd::min(IPC_BUFFER_SIZE - target->buf_size, size);

			target->buf_size += to_write;

			auto remaining_at_end = IPC_BUFFER_SIZE - target->buf_write_ptr;
			auto copy = kstd::min(to_write, remaining_at_end);
			memcpy(target->buf + target->buf_write_ptr, data, copy);
			target->buf_write_ptr += copy;
			size -= copy;

			if (target->buf_write_ptr == IPC_BUFFER_SIZE) {
				target->buf_write_ptr = 0;

				if (copy != to_write) {
					memcpy(target->buf, offset(data, void*, copy), to_write - copy);
					target->buf_write_ptr += to_write - copy;
					size -= to_write - copy;
				}
			}

			data = offset(data, void*, to_write);
		}

		if (!size) {
			break;
		}

		if (flags & SOCK_NONBLOCK) {
			size = orig_size - size;
			return ERR_TRY_AGAIN;
		}

		target->read_event.wait();
	}

	size = orig_size;
	return 0;
}

int IpcSocket::receive(void* data, usize& size) {
	size_t received = 0;
	size_t total = size;
	while (received < total) {
		bool wait;
		{
			IrqGuard irq_guard {};
			auto guard = lock.lock();
			if (!target && !buf_size) {
				return ERR_INVALID_ARGUMENT;
			}
			wait = !buf_size;
		}

		if (wait && (flags & SOCK_NONBLOCK)) {
			if (received) {
				size = received;
				return 0;
			}
			else {
				return ERR_TRY_AGAIN;
			}
		}

		if (wait) {
			read_event.signal_one();
			write_event.wait();
			continue;
		}

		IrqGuard irq_guard {};
		auto guard = lock.lock();

		auto remaining_at_end = IPC_BUFFER_SIZE - buf_read_ptr;
		auto copy = kstd::min(kstd::min(total - received, buf_size), remaining_at_end);
		memcpy(offset(data, void*, received), buf + buf_read_ptr, copy);
		buf_read_ptr += copy;
		buf_size -= copy;
		received += copy;
		if (buf_read_ptr == IPC_BUFFER_SIZE) {
			buf_read_ptr = 0;
		}
	}

	return 0;
}

int IpcSocket::get_peer_name(AnySocketAddress& address) {
	IrqGuard irq_guard {};
	auto guard = lock.lock();
	if (!target) {
		return ERR_INVALID_ARGUMENT;
	}
	assert(target_address.descriptor);

	address.ipc = target_address;
	return 0;
}
