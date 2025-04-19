#pragma once
#include "types.hpp"
#include "crescent/socket.h"
#include "shared_ptr.hpp"
#include "variant.hpp"

struct ProcessDescriptor;

struct KernelIpcSocketAddress {
	SocketAddress generic;
	ProcessDescriptor* descriptor;
};

union AnySocketAddress {
	SocketAddress generic;
	KernelIpcSocketAddress ipc;
	Ipv4SocketAddress ipv4;
	Ipv6SocketAddress ipv6;
};

struct Socket {
	constexpr explicit Socket(int flags) : flags {flags} {}

	virtual ~Socket() = default;

	virtual int connect(const AnySocketAddress& address) = 0;
	virtual int disconnect() = 0;
	virtual int listen(uint32_t port) = 0;
	virtual int accept(kstd::shared_ptr<Socket>& connection, int connection_flags) = 0;
	virtual int send(const void* data, usize& size) = 0;
	virtual int receive(void* data, usize& size) = 0;

	virtual int send_to(const void* data, usize size, const AnySocketAddress& dest) {
		return ERR_UNSUPPORTED;
	}

	virtual int receive_from(void* data, usize& size, AnySocketAddress& src) {
		return ERR_UNSUPPORTED;
	}

	virtual int get_peer_name(AnySocketAddress& address) = 0;

protected:
	int flags;
};
