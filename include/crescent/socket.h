#ifndef CRESCENT_SOCKET_H
#define CRESCENT_SOCKET_H

#include "syscalls.h"
#include <stdint.h>

typedef enum {
	SOCKET_TYPE_IPC,
	SOCKET_TYPE_UDP,
	SOCKET_TYPE_TCP
} SocketType;

typedef enum {
	SOCKET_ADDRESS_TYPE_IPC,
	SOCKET_ADDRESS_TYPE_IPV4,
	SOCKET_ADDRESS_TYPE_IPV6
} SocketAddressType;

typedef struct {
	SocketAddressType type;
} SocketAddress;

typedef struct {
	SocketAddress generic;
	CrescentHandle target;
} IpcSocketAddress;

typedef struct {
	SocketAddress generic;
	uint32_t ipv4;
	uint32_t port;
} Ipv4SocketAddress;

typedef struct {
	SocketAddress generic;
	uint16_t ipv6[8];
	uint32_t port;
} Ipv6SocketAddress;

typedef enum {
	SOCK_NONE = 0,
	SOCK_NONBLOCK = 1 << 0
} SocketFlag;

#endif
