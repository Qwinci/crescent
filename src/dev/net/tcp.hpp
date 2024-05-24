#pragma once
#include "shared_ptr.hpp"
#include "sys/socket.hpp"

struct Nic;
struct ReceivedPacket;

void tcp_process_packet(Nic& nic, ReceivedPacket& packet);
kstd::shared_ptr<Socket> tcp_socket_create(int flags);
