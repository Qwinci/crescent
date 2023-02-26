#pragma once
#include "types.hpp"

struct Nic;

class NetSocket {
public:
	enum Type {
		Udp,
		Tcp
	};

	NetSocket(Nic* nic, u16 port, Type type);
	~NetSocket();

	void send(const void* data, usize size);
	void receive(void* buf, usize size);
private:
	Nic* nic;
	u16 port;
	Type type;
};