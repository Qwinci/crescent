#include "ipc.hpp"
#include "sched/process.hpp"

IpcSocket::IpcSocket(kstd::shared_ptr<ProcessDescriptor> owner_desc, int flags)
	: Socket {flags}, owner_desc {std::move(owner_desc)} {}

IpcSocket::~IpcSocket() {
	disconnect();
}

int IpcSocket::connect(AnySocketAddress& address) {
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
		target_address.descriptor = new ProcessDescriptor {};
		target_address.descriptor->process.get_unsafe() = *process_guard;

		(*process_guard)->add_descriptor(target_address.descriptor);

		auto target_socket_guard = (*process_guard)->ipc_socket.lock();
		if (!*target_socket_guard) {
			auto desc = kstd::make_shared<ProcessDescriptor>();
			desc->process.get_unsafe() = *process_guard;
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
	assert(target->target_address.descriptor == owner_desc.data());
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

int IpcSocket::send(const void* data, usize size) {
	IrqGuard irq_guard {};
	auto guard = lock.lock();

	if (!target) {
		return ERR_INVALID_ARGUMENT;
	}

	while (true) {
		bool wait;
		{
			auto target_guard = target->lock.lock();
			wait = target->buf_size + size > IPC_BUFFER_SIZE;
		}

		if (wait && (flags & SOCK_NONBLOCK)) {
			return ERR_TRY_AGAIN;
		}

		if (wait) {
			target->buf_event.wait();
		}
		else {
			break;
		}
	}

	auto target_guard = target->lock.lock();

	target->buf_size += size;

	auto remaining_at_end = IPC_BUFFER_SIZE - target->buf_write_ptr;
	auto copy = kstd::min(size, remaining_at_end);
	memcpy(target->buf + target->buf_write_ptr, data, copy);
	target->buf_write_ptr += copy;
	size -= copy;
	if (size) {
		target->buf_write_ptr = size;
		memcpy(target->buf, offset(data, void*, copy), size);
	}

	target->buf_event.signal_one();

	return 0;
}

int IpcSocket::receive(void* data, usize& size) {
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
		return ERR_TRY_AGAIN;
	}

	if (wait) {
		buf_event.wait();
	}

	IrqGuard irq_guard {};
	auto guard = lock.lock();

	auto orig_buf_size = buf_size;

	auto remaining_at_end = IPC_BUFFER_SIZE - buf_read_ptr;
	auto copy = kstd::min(kstd::min(size, buf_size), remaining_at_end);
	memcpy(data, buf + buf_read_ptr, copy);
	buf_read_ptr += copy;
	buf_size -= copy;
	if (size - copy && buf_size) {
		buf_read_ptr = buf_size;
		memcpy(offset(data, void*, copy), buf, buf_size);
	}

	size = kstd::min(size, orig_buf_size);

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
