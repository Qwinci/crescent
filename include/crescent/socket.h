#ifndef CRESCENT_SOCKET_H
#define CRESCENT_SOCKET_H

#include "syscalls.h"
#include <stdint.h>

typedef enum SocketType {
	SOCKET_TYPE_IPC,
	SOCKET_TYPE_UDP,
	SOCKET_TYPE_TCP
} SocketType;

typedef enum SocketAddressType {
	SOCKET_ADDRESS_TYPE_IPC,
	SOCKET_ADDRESS_TYPE_IPV4,
	SOCKET_ADDRESS_TYPE_IPV6
} SocketAddressType;

typedef struct SocketAddress {
	SocketAddressType type;
} SocketAddress;

typedef struct IpcSocketAddress {
	SocketAddress generic;
	CrescentHandle target;
} IpcSocketAddress;

typedef struct Ipv4SocketAddress {
	SocketAddress generic;
	uint32_t ipv4;
	uint16_t port;
} Ipv4SocketAddress;

typedef struct Ipv6SocketAddress {
	SocketAddress generic;
	uint16_t ipv6[8];
	uint16_t port;
} Ipv6SocketAddress;

typedef enum SocketFlag {
	SOCK_NONE = 0,
	SOCK_NONBLOCK = 1 << 0
} SocketFlag;

#endif
