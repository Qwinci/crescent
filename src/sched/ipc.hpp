#pragma once
#include "sys/socket.hpp"
#include "dev/event.hpp"

struct IpcSocket final : public Socket {
	~IpcSocket() override;

	static constexpr usize IPC_BUFFER_SIZE = 512;

	int connect(AnySocketAddress& address) override;
	int disconnect() override;
	int listen(uint32_t port) override;
	int accept(kstd::shared_ptr<Socket>& connection) override;
	int send(const void* data, usize size) override;
	int receive(void* data, usize& size) override;

	IpcSocket* pending;
	IpcSocket* target;
	Event pending_event;
	u8 buf[IPC_BUFFER_SIZE];
	usize buf_read_ptr;
	usize buf_write_ptr;
	usize buf_size;
	Event buf_event;
	Spinlock<void> lock;
};
