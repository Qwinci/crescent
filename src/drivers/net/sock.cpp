#include "sock.hpp"

NetSocket::NetSocket(Nic* nic, u16 port, NetSocket::Type type) :
	nic {nic}, port {port}, type {type} {}
