#pragma once
#include "sys/socket.hpp"
#include "dev/event.hpp"

struct IpcSocket final : public Socket {
	IpcSocket(kstd::shared_ptr<ProcessDescriptor> owner_desc, int flags);

	~IpcSocket() override;

	static constexpr usize IPC_BUFFER_SIZE = 512;

	int connect(const AnySocketAddress& address) override;
	int disconnect() override;
	int listen(uint32_t port) override;
	int accept(kstd::shared_ptr<Socket>& connection, int connection_flags) override;
	int send(const void* data, usize size) override;
	int receive(void* data, usize& size) override;

	int get_peer_name(AnySocketAddress& address) override;

	IpcSocket* pending {};
	IpcSocket* target {};
	KernelIpcSocketAddress target_address {};
	kstd::shared_ptr<ProcessDescriptor> owner_desc {};
	Event pending_event {};
	u8 buf[IPC_BUFFER_SIZE] {};
	usize buf_read_ptr {};
	usize buf_write_ptr {};
	usize buf_size {};
	Event buf_event {};
	Spinlock<void> lock {};
};
